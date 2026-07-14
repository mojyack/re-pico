#pragma once
#include "aes.hpp"

namespace crypto {
// rfc 3394 aes key wrap; data_size must be a multiple of 8 and >= 16
// wrapped output is data_size + 8 bytes
auto aes_key_wrap(const Aes128& kek, const u8* data, usize data_size, u8* wrapped) -> bool;
// unwraps wrapped_size bytes into wrapped_size - 8 bytes; false on bad size or icv mismatch
auto aes_key_unwrap(const Aes128& kek, const u8* wrapped, usize wrapped_size, u8* data) -> bool;
} // namespace crypto
