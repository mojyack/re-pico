#pragma once
#include "type-traits.hpp"

namespace noxx {
template <class T>
constexpr auto move(T& v) -> T&& {
    return (T&&)v;
}

template <class T>
constexpr auto forward(RemoveReference<T>& v) -> T&& {
    return (T&&)v;
}

template <class T>
constexpr auto forward(RemoveReference<T>&& v) -> T&& {
    return (T&&)v;
}

template <class T>
constexpr auto destroy_at(T* const ptr) -> void {
    ptr->~T();
}

template <class T, class U = T>
constexpr auto exchange(T& a, U&& b) -> T {
    auto old = move(a);
    a        = move(b);
    return old;
}
} // namespace noxx
