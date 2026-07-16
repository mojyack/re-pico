#pragma once
#if defined(NOXX_TEST)
#include <new>
#else
#include "malloc.hpp"
#endif

#include "utility.hpp"

namespace noxx {
struct NullOpt {
};

constexpr auto nullopt = NullOpt();

template <class T>
struct Optional {
    alignas(T) char data[sizeof(T)];
    bool valid = false;

    template <class... Args>
    auto emplace(Args&&... args) -> T& {
        clear();
        new((T*)data) T(move(args)...);
        valid = true;
        return *(T*)data;
    }

    auto clear() -> void {
        if(valid) {
            ((T*)data)->~T();
            valid = false;
        }
    }

    auto operator->() -> T* {
        return (T*)data;
    }

    auto operator->() const -> T* {
        return (T*)data;
    }

    auto operator*() -> T& {
        return *(T*)data;
    }

    auto operator*() const -> const T& {
        return *(const T*)data;
    }

    operator bool() const {
        return valid;
    }

    auto operator=(Optional&& other) -> Optional& {
        if(this == &other) {
            return *this;
        }
        if(other.valid) {
            emplace(move(*other));
            other.clear();
        } else {
            clear();
        }
        return *this;
    }

    Optional() = default;

    Optional(NullOpt) {
    }

    Optional(T&& v) {
        emplace(move(v));
    }

    Optional(Optional&& other) {
        *this = noxx::forward<Optional&&>(other);
    }

    ~Optional() {
        clear();
    }
};
} // namespace noxx
