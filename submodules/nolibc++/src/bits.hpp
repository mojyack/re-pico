#pragma once
#include "int.hpp"

namespace noxx {
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

static_assert(lsb<1> == 0);
static_assert(lsb<8> == 3);

// exp=0:1byte exp=1:2byte exp=2:4byte ... exp=n:2^nbyte
template <class T>
constexpr auto align_ceil(T data, int exp) -> T {
    auto num = (usize)data;
    num -= 1;
    num &= ~((1 << exp) - 1); // num & 0b1111....1000
    num += 1 << exp;
    return (T)num;
}

template <class T>
constexpr auto align_floor(T data, int exp) -> T {
    auto num = (usize)data;
    num &= ~((1 << exp) - 1); // num & 0b1111....1000
    return (T)num;
}

static_assert(align_ceil(127, lsb<1>) == 127);
static_assert(align_ceil(127, lsb<2>) == 128);
// ...
static_assert(align_ceil(127, lsb<128>) == 128);
static_assert(align_ceil(127, lsb<256>) == 256);

static_assert(align_floor(129, lsb<1>) == 129);
static_assert(align_floor(129, lsb<2>) == 128);
// ...
static_assert(align_floor(129, lsb<128>) == 128);
static_assert(align_floor(129, lsb<256>) == 0);
} // namespace noxx

// num to bitfield
#define BF(field, value) ((value) << noxx::lsb<field>)

// bitfield to num
#define FB(field, value) ((value & field) >> noxx::lsb<field>)
