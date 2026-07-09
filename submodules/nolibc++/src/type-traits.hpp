#pragma once
#include "int.hpp"

namespace noxx {
// common types
template <class T>
struct Tag {
    using Type = T;
};

struct True {
    constexpr static auto value = true;
};

struct False {
    constexpr static auto value = false;
};

// is_same
template <class A, class B>
struct IsSame : False {};

template <class A>
struct IsSame<A, A> : True {};

template <class A, class B>
constexpr auto is_same = IsSame<A, B>::value;

static_assert(is_same<int, int>);
static_assert(!is_same<int, char>);

// conditional
template <bool c, class A, class B>
constexpr auto conditional() -> auto {
    if constexpr(c) {
        return Tag<A>();
    } else {
        return Tag<B>();
    }
}

template <bool c, class A, class B>
using Conditional = decltype(conditional<c, A, B>())::Type;

static_assert(is_same<Conditional<true, int, bool>, int>);
static_assert(is_same<Conditional<false, int, bool>, bool>);

// is_integral
template <class T>
constexpr auto is_integral = is_same<T, i8> ||
                             is_same<T, i16> ||
                             is_same<T, i32> ||
                             is_same<T, i64> ||
                             is_same<T, u8> ||
                             is_same<T, u16> ||
                             is_same<T, u32> ||
                             is_same<T, u64> ||
                             is_same<T, usize> ||
                             false;

template <class T>
concept Integral = is_integral<T>;
} // namespace noxx
