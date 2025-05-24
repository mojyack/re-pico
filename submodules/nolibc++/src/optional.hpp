#pragma once
#if defined(NOXX_TEST)
#include <new>
#endif

#include "move.hpp"

namespace noxx {
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

    auto operator*() -> T& {
        return *(T*)data;
    }

    operator bool() const {
        return valid;
    }

    Optional() = default;
    Optional(T&& v) {
        emplace(move(v));
    }

    ~Optional() {
        clear();
    }
};
} // namespace noxx
