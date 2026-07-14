#pragma once
#include <noxx/format.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

// little-endian buffer accessors and console logging shared by the driver
namespace halow {
inline auto get_u16(const u8* const p) -> u16 {
    return u16(p[0]) | u16(p[1]) << 8;
}

inline auto get_u32(const u8* const p) -> u32 {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

inline auto put_u16(u8* const p, const u16 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
}

inline auto put_u32(u8* const p, const u32 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

template <noxx::comptime::String str, class... Args>
auto log(const Args&... args) -> void {
    auto raw = noxx::format<str>(noxx::move(args)...);
    if(raw) {
        noxx::console_out((*raw).data());
    }
}

inline auto mac_equal(const u8* const a, const u8* const b) -> bool {
    for(auto i = usize(0); i < 6; i += 1) {
        if(a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

// appends little-endian scalars and byte runs to a fixed buffer, tracking overflow
struct Builder {
    noxx::Span<u8> buf;
    usize          offset   = 0;
    bool           overflow = false;

    auto put(const u8 v) -> void {
        if(offset >= buf.size) {
            overflow = true;
            return;
        }
        buf[offset] = v;
        offset += 1;
    }

    auto put_u16(const u16 v) -> void {
        put(u8(v));
        put(u8(v >> 8));
    }

    auto put_u32(const u32 v) -> void {
        put_u16(u16(v));
        put_u16(u16(v >> 16));
    }

    auto put_bytes(const u8* const data, const usize size) -> void {
        for(auto i = usize(0); i < size; i += 1) {
            put(data[i]);
        }
    }

    // begin a tlv, returning the offset of its length field for end_tlv
    auto begin_tlv(const u16 tag) -> usize {
        put_u16(tag);
        const auto len_at = offset;
        put_u16(0);
        return len_at;
    }

    // patch the tlv length with the bytes appended since begin_tlv
    auto end_tlv(const usize len_at) -> void {
        if(overflow) {
            return;
        }
        const auto len  = u16(offset - len_at - 2);
        buf[len_at]     = len;
        buf[len_at + 1] = len >> 8;
    }
};
} // namespace halow
