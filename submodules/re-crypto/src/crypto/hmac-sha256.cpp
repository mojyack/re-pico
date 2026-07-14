#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "hmac-sha256.hpp"

namespace crypto {
namespace {
constexpr auto ipad = u8(0x36);
constexpr auto opad = u8(0x5c);
} // namespace

auto HmacSha256::reset(const u8* const key, const usize key_size) -> void {
    auto key_block = noxx::Array<u8, Sha256::block_size>();
    if(key_size > Sha256::block_size) {
        sha256(key, key_size, key_block.data);
    } else {
        noxx::memcpy(key_block.data, key, key_size);
    }
    inner.reset();
    auto inner_pad = noxx::Array<u8, Sha256::block_size>();
    for(auto i = usize(0); i < Sha256::block_size; i += 1) {
        inner_pad[i] = key_block[i] ^ ipad;
        outer_pad[i] = key_block[i] ^ opad;
    }
    inner.update(inner_pad.data, inner_pad.size());
}

auto HmacSha256::update(const u8* const data, const usize size) -> void {
    inner.update(data, size);
}

auto HmacSha256::finish(u8* const mac) -> void {
    auto inner_digest = noxx::Array<u8, Sha256::digest_size>();
    inner.finish(inner_digest.data);
    auto outer = Sha256();
    outer.update(outer_pad, Sha256::block_size);
    outer.update(inner_digest.data, inner_digest.size());
    outer.finish(mac);
}

auto hmac_sha256(const u8* const key, const usize key_size, const u8* const data, const usize size, u8* const mac) -> void {
    auto ctx = HmacSha256(key, key_size);
    ctx.update(data, size);
    ctx.finish(mac);
}
} // namespace crypto
