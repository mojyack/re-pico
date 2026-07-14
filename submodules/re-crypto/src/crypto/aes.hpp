#pragma once
#include <noxx/int.hpp>

namespace crypto {
struct Aes128 {
    static constexpr auto block_size = usize(16);
    static constexpr auto key_size   = usize(16);
    static constexpr auto num_rounds = usize(10);

    u8 round_keys[(num_rounds + 1) * block_size];

    auto encrypt_block(const u8* in, u8* out) const -> void;
    auto decrypt_block(const u8* in, u8* out) const -> void;

    Aes128(const u8* key /* key_size */);
};
} // namespace crypto
