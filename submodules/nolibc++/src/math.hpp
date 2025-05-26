#pragma once
#include "int.hpp"

namespace noxx {
constexpr auto log(usize num, int base) -> int {
    auto count = 0;
    while(num != 0) {
        count += 1;
        num /= base;
    }
    return count;
}
static_assert(log(128, 2) == 8);
static_assert(log(99, 10) == 2);
static_assert(log(100, 10) == 3);

template <class T>
constexpr auto max(const T& a) -> const T& {
    return a;
}

template <class T, class... Args>
constexpr auto max(const T& a, const T& b, const Args&&... args) -> const T& {
    return max(a > b ? a : b, args...);
}
static_assert(max(1, 2, 3) == 3);
} // namespace noxx
