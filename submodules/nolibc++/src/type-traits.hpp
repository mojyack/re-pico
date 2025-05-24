#pragma once

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
} // namespace noxx
