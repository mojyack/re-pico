#pragma once
#include <noxx/span.hpp>

#include "aes.hpp"

namespace crypto {
// rfc 4493
auto aes_cmac(const Aes128& aes, noxx::Span<const u8> data, Aes128::BlockMutRef mac) -> void;
} // namespace crypto
