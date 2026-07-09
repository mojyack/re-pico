#pragma once
#include "malloc.hpp"
#include "span.hpp"
#include "utility.hpp"

#include "assert.hpp"

namespace noxx {
template <class T>
struct Vector {
    T*    ptr      = nullptr;
    usize length   = 0;
    usize capacity = 0;

    auto size() const -> usize {
        return length;
    }

    auto data() -> T* {
        return ptr;
    }

    auto data() const -> const T* {
        return ptr;
    }

    auto reserve(const usize new_capacity) -> bool {
#define error_act return false
        if(new_capacity <= capacity) {
            return true;
        }
        const auto new_ptr = (T*)malloc(new_capacity * sizeof(T));
        ensure(new_ptr != nullptr);
        for(auto i = usize(0); i < length; i += 1) {
            new(new_ptr + i) T(move(ptr[i]));
            ptr[i].~T();
        }
        free(ptr);
        ptr      = new_ptr;
        capacity = new_capacity;
        return true;
#undef error_act
    }

    auto resize(const usize new_size) -> bool {
#define error_act return false
        if(new_size <= length) {
            // shrink, no allocation
            for(auto i = new_size; i < length; i += 1) {
                ptr[i].~T();
            }
            length = new_size;
            return true;
        }
        if(new_size > capacity) {
            ensure(reserve(new_size * 2)); // over allocation
        }
        for(auto i = length; i < new_size; i += 1) {
            new(ptr + i) T();
        }
        length = new_size;
        return true;
#undef error_act
    }

    auto clear() -> void {
        for(auto i = usize(0); i < length; i += 1) {
            ptr[i].~T();
        }
        free(ptr);
        ptr      = nullptr;
        length   = 0;
        capacity = 0;
    }

    auto append(T item) -> bool {
#define error_act return false
        if(length >= capacity) {
            ensure(reserve(length == 0 ? 4 : length * 2)); // over allocation
        }
        new(ptr + length) T(move(item));
        length += 1;
        return true;
#undef error_act
    }

    auto operator[](const usize i) -> T& {
        return ptr[i];
    }

    auto operator[](const usize i) const -> const T& {
        return (*(Vector*)this)[i];
    }

    auto operator=(Vector&& other) -> Vector& {
        clear();
        ptr            = other.ptr;
        length         = other.length;
        capacity       = other.capacity;
        other.ptr      = nullptr;
        other.length   = 0;
        other.capacity = 0;
        return *this;
    }

    operator Span<T>() {
        return Span<T>{.data = ptr, .size = length};
    }

    operator Span<const T>() const {
        return Span<T>{.data = ptr, .size = length};
    }

    Vector() = default;

    Vector(Vector&& other) {
        *this = move(other);
    }

    ~Vector() {
        clear();
    }
};

template <class T>
Span(Vector<T>&) -> Span<T>;
} // namespace noxx

#include "assert.hpp"
