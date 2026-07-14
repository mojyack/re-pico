#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "sha256.hpp"

namespace crypto {
namespace {
// fractional parts of the cube roots of the first 64 primes (fips 180-4)
constexpr auto round_constants = noxx::Array<u32, 64>{
    0x428a2f98,
    0x71374491,
    0xb5c0fbcf,
    0xe9b5dba5,
    0x3956c25b,
    0x59f111f1,
    0x923f82a4,
    0xab1c5ed5,
    0xd807aa98,
    0x12835b01,
    0x243185be,
    0x550c7dc3,
    0x72be5d74,
    0x80deb1fe,
    0x9bdc06a7,
    0xc19bf174,
    0xe49b69c1,
    0xefbe4786,
    0x0fc19dc6,
    0x240ca1cc,
    0x2de92c6f,
    0x4a7484aa,
    0x5cb0a9dc,
    0x76f988da,
    0x983e5152,
    0xa831c66d,
    0xb00327c8,
    0xbf597fc7,
    0xc6e00bf3,
    0xd5a79147,
    0x06ca6351,
    0x14292967,
    0x27b70a85,
    0x2e1b2138,
    0x4d2c6dfc,
    0x53380d13,
    0x650a7354,
    0x766a0abb,
    0x81c2c92e,
    0x92722c85,
    0xa2bfe8a1,
    0xa81a664b,
    0xc24b8b70,
    0xc76c51a3,
    0xd192e819,
    0xd6990624,
    0xf40e3585,
    0x106aa070,
    0x19a4c116,
    0x1e376c08,
    0x2748774c,
    0x34b0bcb5,
    0x391c0cb3,
    0x4ed8aa4a,
    0x5b9cca4f,
    0x682e6ff3,
    0x748f82ee,
    0x78a5636f,
    0x84c87814,
    0x8cc70208,
    0x90befffa,
    0xa4506ceb,
    0xbef9a3f7,
    0xc67178f2,
};

constexpr auto rotr(const u32 v, const int n) -> u32 {
    return (v >> n) | (v << (32 - n));
}

auto compress(u32* const state, const u8* const block) -> void {
    auto w = noxx::Array<u32, 64>();
    for(auto i = 0; i < 16; i += 1) {
        const auto b = block + i * 4;
        w[i]         = u32(b[0]) << 24 | u32(b[1]) << 16 | u32(b[2]) << 8 | u32(b[3]);
    }
    for(auto i = 16; i < 64; i += 1) {
        const auto s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const auto s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i]          = w[i - 16] + s0 + w[i - 7] + s1;
    }
    auto a = state[0], b = state[1], c = state[2], d = state[3];
    auto e = state[4], f = state[5], g = state[6], h = state[7];
    for(auto i = 0; i < 64; i += 1) {
        const auto s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        const auto ch = (e & f) ^ (~e & g);
        const auto t1 = h + s1 + ch + round_constants[i] + w[i];
        const auto s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        const auto mj = (a & b) ^ (a & c) ^ (b & c);
        const auto t2 = s0 + mj;
        h = g, g = f, f = e;
        e = d + t1;
        d = c, c = b, b = a;
        a = t1 + t2;
    }
    state[0] += a, state[1] += b, state[2] += c, state[3] += d;
    state[4] += e, state[5] += f, state[6] += g, state[7] += h;
}
} // namespace

auto Sha256::reset() -> void {
    // fractional parts of the square roots of the first 8 primes
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
    buf_used = 0;
    total    = 0;
}

auto Sha256::update(const u8* data, usize size) -> void {
    total += size;
    if(buf_used != 0) {
        const auto take = size < block_size - buf_used ? size : block_size - buf_used;
        noxx::memcpy(buf + buf_used, data, take);
        buf_used += take;
        data += take;
        size -= take;
        if(buf_used < block_size) {
            return;
        }
        compress(state, buf);
        buf_used = 0;
    }
    while(size >= block_size) {
        compress(state, data);
        data += block_size;
        size -= block_size;
    }
    if(size != 0) {
        noxx::memcpy(buf, data, size);
        buf_used = size;
    }
}

auto Sha256::finish(u8* const digest) -> void {
    const auto bits = total * 8;
    buf[buf_used]   = 0x80;
    buf_used += 1;
    if(buf_used > block_size - 8) {
        for(auto i = buf_used; i < block_size; i += 1) {
            buf[i] = 0;
        }
        compress(state, buf);
        buf_used = 0;
    }
    for(auto i = buf_used; i < block_size - 8; i += 1) {
        buf[i] = 0;
    }
    for(auto i = usize(0); i < 8; i += 1) {
        buf[block_size - 1 - i] = u8(bits >> (i * 8));
    }
    compress(state, buf);
    for(auto i = usize(0); i < 8; i += 1) {
        digest[i * 4 + 0] = u8(state[i] >> 24);
        digest[i * 4 + 1] = u8(state[i] >> 16);
        digest[i * 4 + 2] = u8(state[i] >> 8);
        digest[i * 4 + 3] = u8(state[i]);
    }
}

auto sha256(const u8* const data, const usize size, u8* const digest) -> void {
    auto ctx = Sha256();
    ctx.update(data, size);
    ctx.finish(digest);
}
} // namespace crypto
