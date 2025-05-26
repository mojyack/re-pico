#pragma once
#include "int.hpp"

namespace noxx {
template <class T, usize N>
struct Array {
    T data[N];

    constexpr auto operator[](const usize i) -> T& {
        return data[i];
    }

    constexpr auto operator[](const usize i) const -> const T& {
        return (*(Array*)this)[i];
    }

    constexpr auto size() -> usize {
        return N;
    }
};
} // namespace noxx
