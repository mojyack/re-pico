#include <noxx/array.hpp>
#include <noxx/platform.hpp>

#include "hmac-sha256.hpp"

namespace crypto {
namespace {
constexpr auto block_size = Sha256::Block::size();
constexpr auto ipad       = u8(0x36);
constexpr auto opad       = u8(0x5c);
} // namespace

auto HmacSha256::reset(const noxx::Span<const u8> key) -> void {
    auto key_block = Sha256::Block();
    if(key.size() > block_size) {
        auto digest = Sha256::Digest();
        sha256(key, digest);
        noxx::memcpy(key_block.data, digest.data, digest.size());
    } else {
        noxx::memcpy(key_block.data, key.data, key.size());
    }
    inner.reset();
    auto inner_pad = Sha256::Block();
    for(auto i = usize(0); i < block_size; i += 1) {
        inner_pad[i] = key_block[i] ^ ipad;
        outer_pad[i] = key_block[i] ^ opad;
    }
    inner.update(inner_pad);
}

auto HmacSha256::update(const noxx::Span<const u8> data) -> void {
    inner.update(data);
}

auto HmacSha256::finish(const Sha256::DigestMutRef mac) -> void {
    auto inner_digest = Sha256::Digest();
    inner.finish(inner_digest);
    auto outer = Sha256();
    outer.update(outer_pad);
    outer.update(inner_digest);
    outer.finish(mac);
}

auto hmac_sha256(const noxx::Span<const u8> key, const noxx::Span<const u8> data, const Sha256::DigestMutRef mac) -> void {
    auto ctx = HmacSha256(key);
    ctx.update(data);
    ctx.finish(mac);
}
} // namespace crypto
