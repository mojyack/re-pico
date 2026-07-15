#pragma once
#include <noxx/span.hpp>

#include "aes.hpp"

namespace crypto {
// rfc 3394 aes key wrap; data.size must be a multiple of 8 and >= 16
// wrapped output is data.size() + 8 bytes
auto aes_key_wrap(const Aes128& kek, noxx::Span<const u8> data, u8* wrapped) -> bool;
// unwraps wrapped_size bytes into wrapped.size() - 8 bytes; false on bad size or icv mismatch
auto aes_key_unwrap(const Aes128& kek, noxx::Span<const u8> wrapped, u8* data) -> bool;
} // namespace crypto
