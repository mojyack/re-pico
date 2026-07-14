#pragma once
#include "sha256.hpp"

namespace crypto {
struct HmacSha256 {
    Sha256 inner;
    u8     outer_pad[Sha256::block_size];

    auto reset(const u8* key, usize key_size) -> void;
    auto update(const u8* data, usize size) -> void;
    auto finish(u8* mac /* Sha256::digest_size */) -> void;

    HmacSha256(const u8* const key, const usize key_size) {
        reset(key, key_size);
    }
};

auto hmac_sha256(const u8* key, usize key_size, const u8* data, usize size, u8* mac) -> void;
} // namespace crypto
