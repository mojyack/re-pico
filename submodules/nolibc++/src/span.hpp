#pragma once
#include "int.hpp"
#include "type-traits.hpp"

namespace noxx {
constexpr auto dynamic_extent = usize(-1);

template <usize N>
struct ExtentStorage {
    constexpr operator usize() const {
        return N;
    }
};

template <>
struct ExtentStorage<dynamic_extent> {
    usize size;

    constexpr ExtentStorage(const usize size = 0) : size(size) {}

    constexpr operator usize() const {
        return size;
    }
};

template <class T, usize N = dynamic_extent>
struct Span {
    T* data = nullptr;

    [[no_unique_address]] ExtentStorage<N> extent = {};

    constexpr auto valid() const -> bool {
        return data != nullptr && size() > 0;
    }

    constexpr auto size() const -> usize {
        return extent;
    }

    constexpr auto subspan(const usize offset, const usize count = dynamic_extent) const -> Span<T> {
        return {data + offset, count != dynamic_extent ? count : size() - offset};
    }

    // allow [T -> const T] and [fixed extent -> dynamic extent]
    template <class T2, usize N2>
        requires((is_same<T2, T> || is_same<T2, const T>) &&
                 (N2 == N || N2 == dynamic_extent) &&
                 !(is_same<T2, T> && N2 == N))
    constexpr operator Span<T2, N2>() const {
        if constexpr(N2 == dynamic_extent) {
            return {data, size()};
        } else {
            return {data};
        }
    }

    constexpr auto operator[](const usize i) -> T& {
        return data[i];
    }

    constexpr auto operator[](const usize i) const -> T& {
        return data[i];
    }

    template <class T2, usize N2>
    constexpr auto operator==(const Span<T2, N2> other) const -> bool {
        if(size() != other.size()) {
            return false;
        }
        for(auto i = usize(0); i < size(); i += 1) {
            if(!(data[i] == other.data[i])) {
                return false;
            }
        }
        return true;
    }
};
static_assert(sizeof(Span<int>) == (sizeof(int*) + sizeof(usize)));
static_assert(sizeof(Span<int, 1>) == (sizeof(int*)));
} // namespace noxx
