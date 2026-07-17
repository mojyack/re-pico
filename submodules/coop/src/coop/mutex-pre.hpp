#pragma once
#include "multi-event.hpp"

namespace coop {
struct Mutex;

struct [[nodiscard]] MutexAwaiter : MultiAWaiter {
    Mutex* mutex;

    auto await_ready() const -> bool;

    MutexAwaiter(Mutex& mutex);
};

struct Mutex {
    MultiEvent event;
    bool       held = false;

    auto lock() -> MutexAwaiter;
    auto unlock() -> void;
};
} // namespace coop
