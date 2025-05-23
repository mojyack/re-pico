#pragma once
#include "int.hpp"

namespace bits {
template <usize num, usize depth>
consteval auto lsb_func() -> usize {
    if constexpr(num & 1) {
        return depth;
    } else {
        return lsb_func<(num >> 1), depth + 1>();
    }
}
template <usize num>
constexpr auto lsb = lsb_func<num, 0>();
} // namespace bits

// num to bitfield
#define BF(field, value) ((value) << bits::lsb<field>)

// bitfield to num
#define FB(field, value) ((value & field) >> bits::lsb<field>)
