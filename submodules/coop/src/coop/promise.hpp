#pragma once
#include <noxx/malloc.hpp>
#include <noxx/utility.hpp>

#include "generator.hpp"
#include "runner-pre.hpp"

namespace coop {
struct PromiseBase {
    Runner* runner = nullptr;

    static auto operator new(const usize size) -> void* {
        return noxx::malloc(size);
    }

    static auto operator delete(void* const ptr) -> void {
        noxx::free(ptr);
    }

    auto initial_suspend() -> std::suspend_always {
        return {};
    }

    auto final_suspend() noexcept -> std::suspend_always {
        return {};
    }

    auto unhandled_exception() -> void {
    }
};

template <class T>
struct Promise : PromiseBase {
    T data;

    auto get_return_object() -> CoGenerator<T> {
        return CoGenerator<T>(*this);
    }

    auto yield_value(T data) -> std::suspend_always {
        this->data = noxx::move(data);
        return {};
    }

    auto return_value(T data) -> void {
        this->data = noxx::move(data);
    }
};

template <>
struct Promise<void> : PromiseBase {
    auto get_return_object() -> CoGenerator<void> {
        return CoGenerator<void>(*this);
    }

    auto return_void() -> void {
    }
};
} // namespace coop
