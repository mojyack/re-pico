#pragma once
#include "int.hpp"

namespace noxx {
constexpr auto byteswap(const u8 num) -> u8 {
    return num;
}

constexpr auto byteswap(const u16 num) -> u16 {
    return __builtin_bswap16(num);
}

constexpr auto byteswap(const u32 num) -> u32 {
    return __builtin_bswap32(num);
}

constexpr auto byteswap(const u64 num) -> u64 {
    return __builtin_bswap64(num);
}
} // namespace noxx
