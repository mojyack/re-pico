// minimal raw-DEFLATE (RFC 1951) decoder.
#include <noxx/array.hpp>
#include <noxx/assert.hpp>
#include <noxx/optional.hpp>

#include "inflate.hpp"

namespace {
constexpr auto max_bits   = u32(15);  // longest huffman code
constexpr auto max_lcodes = u32(286); // literal/length symbols
constexpr auto max_dcodes = u32(30);  // distance symbols

// LSB-first bit reader over the source buffer
struct BitReader {
    noxx::Span<const u8> src;
    u32                  pos;
    u32                  bitbuf;
    u32                  bitcnt;
};

// pull `need` bits (0..24)
auto get_bits(BitReader& r, const u32 need) -> noxx::Optional<u32> {
    constexpr auto error_value = noxx::nullopt;

    auto val = r.bitbuf;
    while(r.bitcnt < need) {
        ensure(r.pos < r.src.size());
        val |= u32(r.src[r.pos]) << r.bitcnt;
        r.pos += 1;
        r.bitcnt += 8;
    }
    r.bitbuf = val >> need;
    r.bitcnt -= need;
    return val & ((u32(1) << need) - 1);
}

// canonical huffman decode table (counts per length + symbols in code order)
struct Huffman {
    u16* count;  // count[len] = number of symbols with code length len
    u16* symbol; // symbols ordered by (length, symbol)
};

auto build(Huffman& h, noxx::Span<const u8> lengths) -> void {
    constexpr auto error_value = false;

    for(auto len = u32(0); len <= max_bits; len += 1) {
        h.count[len] = 0;
    }
    for(auto sym = u32(0); sym < lengths.size(); sym += 1) {
        h.count[lengths[sym]] += 1;
    }
    auto offsets = noxx::Array<u16, max_bits + 1>();
    offsets[1]   = 0;
    for(auto len = u32(1); len < max_bits; len += 1) {
        offsets[len + 1] = offsets[len] + h.count[len];
    }
    for(auto sym = u32(0); sym < lengths.size(); sym += 1) {
        if(lengths[sym] != 0) {
            h.symbol[offsets[lengths[sym]]] = sym;
            offsets[lengths[sym]] += 1;
        }
    }
}

// decode one symbol by walking the code bit by bit (RFC 1951 canonical order)
auto decode(BitReader& r, const Huffman& h) -> noxx::Optional<u32> {
    constexpr auto error_value = noxx::nullopt;

    auto code  = i32(0);
    auto first = i32(0);
    auto index = i32(0);
    for(auto len = u32(1); len <= max_bits; len += 1) {
        unwrap(bit, get_bits(r, 1));
        code |= i32(bit);
        const auto count = i32(h.count[len]);
        if(code - count < first) {
            return h.symbol[index + (code - first)];
        }
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    ensure(false);
}

// length/distance base values and extra-bit counts (RFC 1951 sections 3.2.5)
constexpr auto len_base   = noxx::to_array<u16>({3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258});
constexpr auto len_extra  = noxx::to_array<u8>({0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0});
constexpr auto dist_base  = noxx::to_array<u16>({1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577});
constexpr auto dist_extra = noxx::to_array<u8>({0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13});

struct State {
    BitReader      r;
    noxx::Span<u8> dst;
    u32            dst_pos;
};

// expand one compressed block using the given literal/length + distance codes
auto inflate_codes(State& s, const Huffman& lit, const Huffman& dist) -> bool {
    constexpr auto error_value = false;

    while(true) {
        unwrap(sym, decode(s.r, lit));
        if(sym == 256) {
            return true; // end of block
        }
        if(sym < 256) {
            ensure(s.dst_pos < s.dst.size());
            s.dst[s.dst_pos] = u8(sym);
            s.dst_pos += 1;
            continue;
        }
        sym -= 257;
        ensure(sym < i32(sizeof(len_base) / sizeof(len_base[0])));
        unwrap(extra, get_bits(s.r, len_extra[sym]));
        const auto length = u32(len_base[sym]) + u32(extra);

        unwrap(dsym, decode(s.r, dist));
        ensure(dsym < max_dcodes);
        unwrap(dextra, get_bits(s.r, dist_extra[dsym]));
        const auto distance = u32(dist_base[dsym]) + u32(dextra);
        ensure(distance <= s.dst_pos && s.dst_pos + length <= s.dst.size());
        for(auto i = u32(0); i < length; i += 1) {
            s.dst[s.dst_pos] = s.dst[s.dst_pos - distance];
            s.dst_pos += 1;
        }
    }
}

auto inflate_fixed(State& s) -> bool {
    auto lengths = noxx::Array<u8, 288>();
    for(auto sym = u32(0); sym < 144; sym += 1) {
        lengths[sym] = 8;
    }
    for(auto sym = u32(144); sym < 256; sym += 1) {
        lengths[sym] = 9;
    }
    for(auto sym = u32(256); sym < 280; sym += 1) {
        lengths[sym] = 7;
    }
    for(auto sym = u32(280); sym < 288; sym += 1) {
        lengths[sym] = 8;
    }
    auto lit_count  = noxx::Array<u16, max_bits + 1>();
    auto lit_symbol = noxx::Array<u16, 288>();
    auto lit        = Huffman{lit_count.data, lit_symbol.data};
    build(lit, lengths);

    auto dist_lengths = noxx::Array<u8, max_dcodes>();
    for(auto sym = u32(0); sym < max_dcodes; sym += 1) {
        dist_lengths[sym] = 5;
    }
    auto dist_count  = noxx::Array<u16, max_bits + 1>();
    auto dist_symbol = noxx::Array<u16, max_dcodes>();
    auto dist        = Huffman{dist_count.data, dist_symbol.data};
    build(dist, dist_lengths);

    return inflate_codes(s, lit, dist);
}

auto inflate_dynamic(State& s) -> bool {
    constexpr auto error_value = false;

    unwrap(hlit, get_bits(s.r, 5));
    unwrap(hdist, get_bits(s.r, 5));
    unwrap(hclen, get_bits(s.r, 4));
    const auto nlit  = u32(hlit) + 257;
    const auto ndist = u32(hdist) + 1;
    const auto ncode = u32(hclen) + 4;
    ensure(nlit <= max_lcodes && ndist <= max_dcodes);

    // code lengths for the code-length alphabet, in their shuffled order
    constexpr auto order      = noxx::to_array<u8>({16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15});
    auto           cl_lengths = noxx::Array<u8, order.size()>();
    for(auto i = u32(0); i < ncode; i += 1) {
        unwrap(v, get_bits(s.r, 3));
        cl_lengths[order[i]] = u8(v);
    }
    auto cl_count  = noxx::Array<u16, max_bits + 1>();
    auto cl_symbol = noxx::Array<u16, 19>();
    auto cl        = Huffman{cl_count.data, cl_symbol.data};
    build(cl, cl_lengths);

    // decode the literal/length and distance code lengths (with run-length ops)
    auto       lengths = noxx::Array<u8, max_lcodes + max_dcodes>();
    const auto total   = nlit + ndist;
    auto       idx     = u32(0);
    while(idx < total) {
        unwrap(sym, decode(s.r, cl));
        if(sym < 16) {
            lengths[idx] = u8(sym);
            idx += 1;
            continue;
        }
        auto repeat = u32(0);
        auto value  = u8(0);
        if(sym == 16) {
            if(idx == 0) {
                return false;
            }
            value = lengths[idx - 1];
            unwrap(n, get_bits(s.r, 2));
            repeat = 3 + u32(n);
        } else if(sym == 17) {
            unwrap(n, get_bits(s.r, 3));
            repeat = 3 + u32(n);
        } else { // sym == 18
            unwrap(n, get_bits(s.r, 7));
            repeat = 11 + u32(n);
        }
        if(idx + repeat > total) {
            return false;
        }
        for(auto i = u32(0); i < repeat; i += 1) {
            lengths[idx] = value;
            idx += 1;
        }
    }

    auto lit_count  = noxx::Array<u16, max_bits + 1>();
    auto lit_symbol = noxx::Array<u16, max_lcodes>();
    auto lit        = Huffman{lit_count.data, lit_symbol.data};
    build(lit, noxx::Span<const u8>(lengths).subspan(0, nlit));
    auto dist_count  = noxx::Array<u16, max_bits + 1>();
    auto dist_symbol = noxx::Array<u16, max_dcodes>();
    auto dist        = Huffman{dist_count.data, dist_symbol.data};
    build(dist, noxx::Span<const u8>(lengths).subspan(nlit, ndist));

    return inflate_codes(s, lit, dist);
}

// copy an uncompressed (stored) block after aligning to the next byte boundary
auto inflate_stored(State& s) -> bool {
    s.r.bitbuf = 0;
    s.r.bitcnt = 0;
    if(s.r.pos + 4 > s.r.src.size()) {
        return false;
    }
    const auto len = u32(s.r.src[s.r.pos]) | u32(s.r.src[s.r.pos + 1]) << 8;
    s.r.pos += 4; // skip LEN + its one's-complement NLEN
    if(s.r.pos + len > s.r.src.size() || s.dst_pos + len > s.dst.size()) {
        return false;
    }
    for(auto i = u32(0); i < len; i += 1) {
        s.dst[s.dst_pos] = s.r.src[s.r.pos];
        s.dst_pos += 1;
        s.r.pos += 1;
    }
    return true;
}
} // namespace

auto inflate(const noxx::Span<const u8> src, const noxx::Span<u8> dst) -> bool {
    constexpr auto error_value = false;

    auto s = State{
        BitReader{src.data, src.size(), 0, 0, 0},
        dst,
        0,
    };
    while(true) {
        unwrap(last, get_bits(s.r, 1));
        unwrap(type, get_bits(s.r, 2));
        switch(type) {
        case 0:
            ensure(inflate_stored(s));
            break;
        case 1:
            ensure(inflate_fixed(s));
            break;
        case 2:
            ensure(inflate_dynamic(s));
            break;
        default: // 3 is reserved
            ensure(false);
        }
        if(last != 0) {
            break;
        }
    }
    return s.dst_pos == dst.size();
}
