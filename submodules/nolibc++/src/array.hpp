#pragma once
#include "integer-sequence.hpp"
#include "span.hpp"
#include "utility.hpp"

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

    operator Span<T>() {
        return Span<T>{.data = data, .size = N};
    }

    operator Span<const T>() const {
        return Span<const T>{.data = data, .size = N};
    }

    constexpr auto size() const -> usize {
        return N;
    }
};

template <class T, usize N, usize... I>
constexpr auto to_array_impl(T (&&list)[N], IntegerSequence<usize, I...>) -> Array<T, N> {
    return {move(list[I])...};
}

template <class T, usize N>
constexpr auto to_array(T (&&list)[N]) -> Array<T, N> {
    return to_array_impl(move(list), make_integer_sequence<usize, N>{});
}

template <class T, usize N, usize... I>
constexpr auto to_array_impl(const T (&list)[N], IntegerSequence<usize, I...>) -> Array<T, N> {
    return {list[I]...};
}

template <class T, usize N>
constexpr auto to_array(const T (&list)[N]) -> Array<T, N> {
    return to_array_impl(list, make_integer_sequence<usize, N>{});
}

template <class T, usize N>
Span(Array<T, N>&) -> Span<T>;
} // namespace noxx
