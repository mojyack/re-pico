#pragma once
#include <noxx/utility.hpp>

#include "multi-event.hpp"
#include "mutex-pre.hpp"

namespace coop {
inline auto MutexAwaiter::await_ready() const -> bool {
    return !noxx::exchange(mutex->held, true);
}

template <CoHandleLike CoHandle>
inline auto MutexAwaiter::await_suspend(CoHandle caller_task) -> void {
    mutex->event.await_suspend(noxx::move(caller_task));
}

inline auto Mutex::lock() -> MutexAwaiter {
    return MutexAwaiter(this);
}

inline auto Mutex::unlock() -> void {
    if(event.waiters.size() == 0) {
        held = false;
    } else {
        event.notify(1); // hand ownership to next waiter
    }
}
} // namespace coop
