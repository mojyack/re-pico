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

template <class A, class B>
concept same_as = is_same<A, B>;

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

// remove_reference
template <class T>
struct RemoveReferenceI : Tag<T> {};

template <class T>
struct RemoveReferenceI<T&> : Tag<T> {};

template <class T>
struct RemoveReferenceI<T&&> : Tag<T> {};

template <class T>
using RemoveReference = RemoveReferenceI<T>::Type;

static_assert(is_same<RemoveReference<int&>, int>);
static_assert(is_same<RemoveReference<int&&>, int>);
static_assert(is_same<RemoveReference<int>, int>);

// remove_cv
template <class T>
struct RemoveCvI : Tag<T> {};

template <class T>
struct RemoveCvI<const T> : Tag<T> {};

template <class T>
struct RemoveCvI<volatile T> : Tag<T> {};

template <class T>
struct RemoveCvI<const volatile T> : Tag<T> {};

template <class T>
using RemoveCv = RemoveCvI<T>::Type;

static_assert(is_same<RemoveCv<const int>, int>);
static_assert(is_same<RemoveCv<const volatile int>, int>);

// remove_cvref
template <class T>
using RemoveCvRef = RemoveCv<RemoveReference<T>>;

static_assert(is_same<RemoveCvRef<const int&>, int>);
static_assert(is_same<RemoveCvRef<int&&>, int>);

// tuple_element (for type packs)
template <usize n, class... Ts>
using NthType = __type_pack_element<n, Ts...>;

static_assert(is_same<NthType<0, int, bool>, int>);
static_assert(is_same<NthType<1, int, bool>, bool>);

// is_constructible
template <class T, class... Args>
constexpr auto is_constructible = __is_constructible(T, Args...);

static_assert(is_constructible<int, int>);
static_assert(!is_constructible<int, int*>);

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
