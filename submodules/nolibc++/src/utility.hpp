#pragma once

namespace noxx {
template <class T>
auto move(T& v) -> T&& {
    return (T&&)v;
}

template <class T, class U = T>
auto exchange(T& a, U&& b) -> T {
    auto old = move(a);
    a        = move(b);
    return old;
}
} // namespace noxx
