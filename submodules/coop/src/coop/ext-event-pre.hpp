#pragma once
#include "cohandle.hpp"
#include "event.hpp"

namespace coop {
struct Runner;
struct Task;
struct ExtAWaiter;

struct ExtEvent {
    Task*       waiter = nullptr;
    ExtAWaiter* awaiter;

    // call when available() changed
    auto notify() -> void;

    virtual auto available() const -> bool = 0;
};

struct [[nodiscard]] ExtAWaiter {
    Runner*     runner;
    ExtEvent*   event;
    Duration    timeout;
    EventResult result;

    auto await_ready() -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> EventResult;
};

auto wait_for_event(ExtEvent& event, Duration timeout = time_infinite) -> ExtAWaiter;
} // namespace coop
