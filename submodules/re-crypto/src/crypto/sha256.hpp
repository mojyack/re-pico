#pragma once
#include <noxx/int.hpp>

namespace crypto {
struct Sha256 {
    static constexpr auto block_size  = usize(64);
    static constexpr auto digest_size = usize(32);

    u32   state[8];
    u8    buf[block_size];
    usize buf_used;
    u64   total;

    auto reset() -> void;
    auto update(const u8* data, usize size) -> void;
    auto finish(u8* digest /* digest_size */) -> void;

    Sha256() {
        reset();
    }
};

auto sha256(const u8* data, usize size, u8* digest) -> void;
} // namespace crypto
