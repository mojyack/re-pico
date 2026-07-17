#pragma once
#include <noxx/utility.hpp>

#include "mutex-pre.hpp"

namespace coop {
inline auto MutexAwaiter::await_ready() const -> bool {
    return !noxx::exchange(mutex->held, true);
}

inline MutexAwaiter::MutexAwaiter(Mutex& m) {
    event   = &m.event;
    timeout = time_infinite;
    mutex   = &m;
}

inline auto Mutex::lock() -> MutexAwaiter {
    return MutexAwaiter(*this);
}

inline auto Mutex::unlock() -> void {
    if(event.waiters.size() == 0) {
        held = false;
    } else {
        event.notify(1); // hand ownership to next waiter
    }
}
} // namespace coop
