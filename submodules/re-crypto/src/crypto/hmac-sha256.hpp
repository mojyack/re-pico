#pragma once
#include "sha256.hpp"

namespace crypto {
struct HmacSha256 {
    Sha256        inner;
    Sha256::Block outer_pad;

    auto reset(noxx::Span<const u8> key) -> void;
    auto update(noxx::Span<const u8> data) -> void;
    auto finish(Sha256::DigestMutRef mac) -> void;

    HmacSha256(const noxx::Span<const u8> key) {
        reset(key);
    }
};

auto hmac_sha256(noxx::Span<const u8> key, noxx::Span<const u8> data, Sha256::DigestMutRef mac) -> void;
} // namespace crypto
