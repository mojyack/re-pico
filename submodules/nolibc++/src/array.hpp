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
        return {data, N};
    }

    operator Span<const T>() const {
        return {data, N};
    }

    operator Span<T, N>() {
        return {data};
    }

    operator Span<const T, N>() const {
        return {data};
    }

    constexpr auto operator==(const Array& other) const -> bool {
        return Span<const T>(*this) == Span<const T>(other);
    }

    constexpr static auto size() -> usize {
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
Span(Array<T, N>&) -> Span<T, N>;

template <class T, usize N, class U, usize M>
    requires(is_same<RemoveCv<U>, T>)
constexpr auto operator==(const Array<T, N>& array, const Span<U, M> span) -> bool {
    return Span<const T>(array) == span;
}
} // namespace noxx
