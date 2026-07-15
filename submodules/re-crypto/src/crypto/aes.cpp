#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "aes.hpp"

namespace crypto {
namespace {
// gf(2^8) with the aes polynomial x^8+x^4+x^3+x+1
constexpr auto gf_mul(u8 a, u8 b) -> u8 {
    auto r = u8(0);
    for(auto i = 0; i < 8; i += 1) {
        if(b & 1) {
            r ^= a;
        }
        const auto hi = a & 0x80;
        a <<= 1;
        if(hi) {
            a ^= 0x1b;
        }
        b >>= 1;
    }
    return r;
}

constexpr auto gf_inv(const u8 a) -> u8 {
    // a^254 == a^-1 (a^255 == 1 for a != 0); gf_inv(0) = 0 as required by the sbox
    auto r = u8(1);
    auto p = a;
    auto e = 254;
    while(e != 0) {
        if(e & 1) {
            r = gf_mul(r, p);
        }
        p = gf_mul(p, p);
        e >>= 1;
    }
    return r;
}

constexpr auto rotl8(const u8 v, const int n) -> u8 {
    return u8(v << n | v >> (8 - n));
}

// sbox generated from its definition (inverse + affine transform) to avoid
// transcription errors; spot-checked against fips-197 below
constexpr auto sbox = [] {
    auto t = noxx::Array<u8, 256>();
    for(auto i = usize(0); i < 256; i += 1) {
        const auto b = gf_inv(u8(i));
        t[i]         = u8(b ^ rotl8(b, 1) ^ rotl8(b, 2) ^ rotl8(b, 3) ^ rotl8(b, 4) ^ 0x63);
    }
    return t;
}();
static_assert(sbox[0x00] == 0x63 && sbox[0x01] == 0x7c && sbox[0x53] == 0xed && sbox[0xff] == 0x16);

constexpr auto inv_sbox = [] {
    auto t = noxx::Array<u8, 256>();
    for(auto i = usize(0); i < 256; i += 1) {
        t[sbox[i]] = u8(i);
    }
    return t;
}();
static_assert(inv_sbox[0x63] == 0x00 && inv_sbox[0xed] == 0x53);

constexpr auto round_consts = noxx::Array<u8, 10>{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

auto add_round_key(const Aes128::BlockMutRef state, const Aes128::BlockRef round_key) -> void {
    for(auto i = usize(0); i < state.size(); i += 1) {
        state[i] ^= round_key[i];
    }
}

auto sub_bytes(const Aes128::BlockMutRef s) -> void {
    for(auto i = usize(0); i < s.size(); i += 1) {
        s[i] = sbox[s[i]];
    }
}

auto inv_sub_bytes(const Aes128::BlockMutRef s) -> void {
    for(auto i = usize(0); i < s.size(); i += 1) {
        s[i] = inv_sbox[s[i]];
    }
}

// state layout: s[r + 4c] = row r, column c (column-major, as in fips-197)
auto shift_rows(const Aes128::BlockMutRef s) -> void {
    auto t = u8();
    t = s[1], s[1] = s[5], s[5] = s[9], s[9] = s[13], s[13] = t;
    t = s[2], s[2] = s[10], s[10] = t;
    t = s[6], s[6] = s[14], s[14] = t;
    t = s[3], s[3] = s[15], s[15] = s[11], s[11] = s[7], s[7] = t;
}

auto inv_shift_rows(const Aes128::BlockMutRef s) -> void {
    auto t = u8();
    t = s[13], s[13] = s[9], s[9] = s[5], s[5] = s[1], s[1] = t;
    t = s[2], s[2] = s[10], s[10] = t;
    t = s[6], s[6] = s[14], s[14] = t;
    t = s[7], s[7] = s[11], s[11] = s[15], s[15] = s[3], s[3] = t;
}

auto mix_columns(const Aes128::BlockMutRef s) -> void {
    for(auto c = usize(0); c < 4; c += 1) {
        const auto i  = c * 4;
        const auto a0 = s[i], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
        s[i + 0] = gf_mul(a0, 2) ^ gf_mul(a1, 3) ^ a2 ^ a3;
        s[i + 1] = a0 ^ gf_mul(a1, 2) ^ gf_mul(a2, 3) ^ a3;
        s[i + 2] = a0 ^ a1 ^ gf_mul(a2, 2) ^ gf_mul(a3, 3);
        s[i + 3] = gf_mul(a0, 3) ^ a1 ^ a2 ^ gf_mul(a3, 2);
    }
}

auto inv_mix_columns(const Aes128::BlockMutRef s) -> void {
    for(auto c = usize(0); c < 4; c += 1) {
        const auto i  = c * 4;
        const auto a0 = s[i], a1 = s[i + 1], a2 = s[i + 2], a3 = s[i + 3];
        s[i + 0] = gf_mul(a0, 14) ^ gf_mul(a1, 11) ^ gf_mul(a2, 13) ^ gf_mul(a3, 9);
        s[i + 1] = gf_mul(a0, 9) ^ gf_mul(a1, 14) ^ gf_mul(a2, 11) ^ gf_mul(a3, 13);
        s[i + 2] = gf_mul(a0, 13) ^ gf_mul(a1, 9) ^ gf_mul(a2, 14) ^ gf_mul(a3, 11);
        s[i + 3] = gf_mul(a0, 11) ^ gf_mul(a1, 13) ^ gf_mul(a2, 9) ^ gf_mul(a3, 14);
    }
}
} // namespace

Aes128::Aes128(const KeyRef key) {
    // word wi (0..43) lives at round_keys[wi / 4][(wi % 4) * 4 ..]
    const auto word = [this](const usize wi) -> u8* {
        return round_keys[wi / 4].data + (wi % 4) * 4;
    };
    noxx::memcpy(round_keys[0].data, key.data, key.size());
    for(auto i = Key::size() / 4; i < (num_rounds + 1) * 4; i += 1) {
        auto t = noxx::Array<u8, 4>();
        noxx::memcpy(t.data, word(i - 1), 4);
        if(i % 4 == 0) {
            const auto t0 = t[0];
            t[0]          = u8(sbox[t[1]] ^ round_consts[i / 4 - 1]);
            t[1]          = sbox[t[2]];
            t[2]          = sbox[t[3]];
            t[3]          = sbox[t0];
        }
        const auto w    = word(i);
        const auto prev = word(i - 4);
        for(auto j = usize(0); j < 4; j += 1) {
            w[j] = prev[j] ^ t[j];
        }
    }
}

auto Aes128::encrypt_block(const BlockRef in, const BlockMutRef out) const -> void {
    auto s = Block();
    noxx::memcpy(s.data, in.data, in.size());
    add_round_key(s, round_keys[0]);
    for(auto round = usize(1); round < num_rounds; round += 1) {
        sub_bytes(s);
        shift_rows(s);
        mix_columns(s);
        add_round_key(s, round_keys[round]);
    }
    sub_bytes(s);
    shift_rows(s);
    add_round_key(s, round_keys[num_rounds]);
    noxx::memcpy(out.data, s.data, s.size());
}

auto Aes128::decrypt_block(const BlockRef in, const BlockMutRef out) const -> void {
    auto s = Block();
    noxx::memcpy(s.data, in.data, in.size());
    add_round_key(s, round_keys[num_rounds]);
    for(auto round = num_rounds - 1; round > 0; round -= 1) {
        inv_shift_rows(s);
        inv_sub_bytes(s);
        add_round_key(s, round_keys[round]);
        inv_mix_columns(s);
    }
    inv_shift_rows(s);
    inv_sub_bytes(s);
    add_round_key(s, round_keys[0]);
    noxx::memcpy(out.data, s.data, s.size());
}
} // namespace crypto
