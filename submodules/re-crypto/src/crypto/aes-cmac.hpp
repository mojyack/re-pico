#pragma once
#include "aes.hpp"

namespace crypto {
// rfc 4493
auto aes_cmac(const Aes128& aes, const u8* data, usize size, u8* mac /* Aes128::block_size */) -> void;
} // namespace crypto
