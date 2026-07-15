#pragma once
#include <noxx/array.hpp>

#include "util.hpp"

namespace crypto {
struct Sha256 {
    bytes_alias(Block, 64);
    bytes_alias(Digest, 32);

    u32   state[8];
    Block buf;
    usize buf_used;
    u64   total;

    auto reset() -> void;
    auto update(noxx::Span<const u8> data) -> void;
    auto finish(DigestMutRef digest) -> void;

    Sha256() {
        reset();
    }
};

auto sha256(noxx::Span<const u8> data, Sha256::DigestMutRef digest) -> void;
} // namespace crypto
