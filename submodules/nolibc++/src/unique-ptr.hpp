#pragma once
#include "int.hpp"
#include "malloc.hpp"
#include "utility.hpp"

namespace noxx {
template <class T>
struct DefaultDelete {
    auto operator()(T* const ptr) const -> void {
        ptr->~T();
        free(ptr);
    }
};

template <class T>
struct DefaultDelete<T[]> {
    auto operator()(T* const ptr, const usize length) const -> void {
        for(auto i = usize(0); i < length; i += 1) {
            ptr[i].~T();
        }
        free(ptr);
    }
};

template <class T, class Deleter = DefaultDelete<T>>
struct UniquePtr {
    T* ptr = nullptr;

    [[no_unique_address]] Deleter deleter;

    auto get() const -> T* {
        return ptr;
    }

    auto get_deleter() -> Deleter& {
        return deleter;
    }

    auto get_deleter() const -> const Deleter& {
        return deleter;
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

    auto release() -> T* {
        return exchange(ptr, nullptr);
    }

    auto reset() -> void {
        if(ptr != nullptr) {
            deleter(ptr);
            ptr = nullptr;
        }
    }

    auto operator=(UniquePtr&& other) -> UniquePtr& {
        reset();
        ptr     = exchange(other.ptr, nullptr);
        deleter = move(other.deleter);
        return *this;
    }

    UniquePtr() = default;

    explicit UniquePtr(T* const ptr)
        : ptr(ptr) {
    }

    UniquePtr(T* const ptr, Deleter deleter)
        : ptr(ptr),
          deleter(move(deleter)) {
    }

    UniquePtr(UniquePtr&& other) {
        *this = move(other);
    }

    ~UniquePtr() {
        reset();
    }
};

template <class T, class Deleter>
struct UniquePtr<T[], Deleter> {
    T*    ptr    = nullptr;
    usize length = 0;

    [[no_unique_address]] Deleter deleter;

    auto get() const -> T* {
        return ptr;
    }

    auto get_deleter() -> Deleter& {
        return deleter;
    }

    auto get_deleter() const -> const Deleter& {
        return deleter;
    }

    auto release() -> T* {
        length = 0;
        return exchange(ptr, nullptr);
    }

    auto reset() -> void {
        if(ptr != nullptr) {
            deleter(ptr, length);
            ptr    = nullptr;
            length = 0;
        }
    }

    auto operator[](const usize i) const -> T& {
        return ptr[i];
    }

    explicit operator bool() const {
        return ptr != nullptr;
    }

    auto operator=(UniquePtr&& other) -> UniquePtr& {
        reset();
        ptr     = exchange(other.ptr, nullptr);
        length  = exchange(other.length, 0);
        deleter = move(other.deleter);
        return *this;
    }

    UniquePtr() = default;

    UniquePtr(T* const ptr, const usize length)
        : ptr(ptr),
          length(length) {
    }

    UniquePtr(T* const ptr, const usize length, Deleter deleter)
        : ptr(ptr),
          length(length),
          deleter(move(deleter)) {
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
