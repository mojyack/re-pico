// minimal freestanding replacement for <coroutine>
#pragma once
#include <noxx/int.hpp>

namespace std {
template <class R, class... Args>
struct coroutine_traits {
    using promise_type = typename R::promise_type;
};

template <class Promise = void>
struct coroutine_handle;

template <>
struct coroutine_handle<void> {
    void* ptr = nullptr;

    static auto from_address(void* const addr) -> coroutine_handle {
        auto handle = coroutine_handle();
        handle.ptr  = addr;
        return handle;
    }

    auto address() const -> void* {
        return ptr;
    }

    auto done() const -> bool {
        return __builtin_coro_done(ptr);
    }

    auto resume() const -> void {
        __builtin_coro_resume(ptr);
    }

    auto destroy() const -> void {
        __builtin_coro_destroy(ptr);
    }

    auto operator==(const coroutine_handle& other) const -> bool = default;

    explicit operator bool() const {
        return ptr != nullptr;
    }

    coroutine_handle() = default;

    coroutine_handle(decltype(nullptr)) {
    }
};

template <class Promise>
struct coroutine_handle : coroutine_handle<void> {
    static auto from_address(void* const addr) -> coroutine_handle {
        auto handle = coroutine_handle();
        handle.ptr  = addr;
        return handle;
    }

    static auto from_promise(Promise& promise) -> coroutine_handle {
        auto handle = coroutine_handle();
        handle.ptr  = __builtin_coro_promise(&promise, alignof(Promise), true);
        return handle;
    }

    auto promise() const -> Promise& {
        return *(Promise*)__builtin_coro_promise(ptr, alignof(Promise), false);
    }

    coroutine_handle() = default;

    coroutine_handle(decltype(nullptr)) {
    }
};

struct suspend_never {
    auto await_ready() const noexcept -> bool {
        return true;
    }

    auto await_suspend(coroutine_handle<>) const noexcept -> void {
    }

    auto await_resume() const noexcept -> void {
    }
};

struct suspend_always {
    auto await_ready() const noexcept -> bool {
        return false;
    }

    auto await_suspend(coroutine_handle<>) const noexcept -> void {
    }

    auto await_resume() const noexcept -> void {
    }
};
} // namespace std
