#pragma once
#include "int.hpp"
#include "malloc.hpp"
#include "utility.hpp"

namespace noxx {
template <class T>
struct UniquePtr {
    T* ptr = nullptr;

    auto get() const -> T* {
        return ptr;
    }

    auto operator->() const -> T* {
        return ptr;
    }

    auto operator*() const -> T& {
        return *ptr;
    }

    explicit operator bool() const {
        return ptr != nullptr;
    }

    auto reset() -> void {
        if(ptr != nullptr) {
            ptr->~T();
            free(ptr);
            ptr = nullptr;
        }
    }

    auto operator=(UniquePtr&& other) -> UniquePtr& {
        reset();
        ptr = exchange(other.ptr, nullptr);
        return *this;
    }

    UniquePtr() = default;

    explicit UniquePtr(T* const ptr)
        : ptr(ptr) {
    }

    UniquePtr(UniquePtr&& other) {
        *this = move(other);
    }

    ~UniquePtr() {
        reset();
    }
};

template <class T>
struct UniquePtr<T[]> {
    T*    ptr    = nullptr;
    usize length = 0;

    auto get() const -> T* {
        return ptr;
    }

    auto operator[](const usize i) const -> T& {
        return ptr[i];
    }

    explicit operator bool() const {
        return ptr != nullptr;
    }

    auto reset() -> void {
        if(ptr != nullptr) {
            for(auto i = usize(0); i < length; i += 1) {
                ptr[i].~T();
            }
            free(ptr);
            ptr    = nullptr;
            length = 0;
        }
    }

    auto operator=(UniquePtr&& other) -> UniquePtr& {
        reset();
        ptr    = exchange(other.ptr, nullptr);
        length = exchange(other.length, 0);
        return *this;
    }

    UniquePtr() = default;

    UniquePtr(T* const ptr, const usize length)
        : ptr(ptr), length(length) {
    }

    UniquePtr(UniquePtr&& other) {
        *this = move(other);
    }

    ~UniquePtr() {
        reset();
    }
};

template <class T, class... Args>
auto make_unique(Args&&... args) -> UniquePtr<T> {
    const auto ptr = (T*)malloc(sizeof(T));
    if(ptr == nullptr) {
        return {};
    }
    new(ptr) T(move(args)...);
    return UniquePtr<T>(ptr);
}

template <class T>
auto make_unique_array(const usize length) -> UniquePtr<T[]> {
    const auto ptr = (T*)malloc(length * sizeof(T));
    if(ptr == nullptr) {
        return {};
    }
    for(auto i = usize(0); i < length; i += 1) {
        new(ptr + i) T();
    }
    return UniquePtr<T[]>(ptr, length);
}
} // namespace noxx
