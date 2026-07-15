#pragma once
#include <noxx/array.hpp>

#include "util.hpp"

namespace crypto {
struct Aes128 {
    bytes_alias(Key, 16);
    bytes_alias(Block, 16);

    static constexpr auto num_rounds = usize(10);

    noxx::Array<Block, num_rounds + 1> round_keys;

    auto encrypt_block(BlockRef in, BlockMutRef out) const -> void;
    auto decrypt_block(BlockRef in, BlockMutRef out) const -> void;

    Aes128(KeyRef key);
};
} // namespace crypto
