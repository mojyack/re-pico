#pragma once

namespace noxx {
template <class T>
constexpr auto max(const T& a, const T& b) -> const T& {
    return a > b ? a : b;
}

template <class T>
constexpr auto min(const T& a, const T& b) -> const T& {
    return a < b ? a : b;
}
} // namespace noxx
