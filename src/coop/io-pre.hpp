#pragma once
#include "cohandle.hpp"

namespace coop {
struct Runner;
struct Task;

inline volatile auto any_io_event_available = false;

struct IOEvent {
    Task* waiter = nullptr;

    // call when available() changed
    auto notify() const -> void;

    virtual auto available() const -> bool = 0;
};

struct [[nodiscard]] IOAWaiter {
    Runner*  runner;
    IOEvent* event;
    bool     result = true;

    auto await_ready() -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> bool;
    IOAWaiter(IOEvent& event);
};

using wait_for_io = IOAWaiter;
} // namespace coop
