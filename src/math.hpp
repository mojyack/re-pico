#pragma once
#include "int.hpp"

namespace math {
template <usize num, usize depth>
consteval auto log2_func() -> usize {
    if constexpr(num & 1) {
        return depth;
    } else {
        return log2_func<(num >> 1), depth + 1>();
    }
}
template <usize num>
constexpr auto log2 = log2_func<num, 0>();
}
