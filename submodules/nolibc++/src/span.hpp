#pragma once
#include "int.hpp"

namespace noxx {
template <class T>
struct Span {
    T*    data;
    usize size;

    constexpr auto subspan(const usize offset, const int count = -1) -> Span<T> {
        return Span<T>{
            .data = data + offset,
            .size = count >= 0 ? count : size - offset,
        };
    }

    operator Span<const T>() {
        return {data, size};
    }

    constexpr auto operator[](const usize i) -> T& {
        return data[i];
    }

    constexpr auto operator[](const usize i) const -> T& {
        return data[i];
    }
};
} // namespace noxx
