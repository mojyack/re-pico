#pragma once
#include "cohandle.hpp"
#include "event.hpp"

namespace coop {
struct Task;
struct Runner;
struct SingleAWaiter;

struct SingleEvent {
    constexpr static auto notified = usize(-1);

    Task*          waiter = nullptr;
    SingleAWaiter* awaiter;

    auto notify() -> void;
};

struct [[nodiscard]] SingleAWaiter {
    Runner*      runner;
    SingleEvent* event;
    Duration     timeout;
    bool         result;

    auto await_ready() -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> bool; // false on timeout
};

auto wait_for_event(SingleEvent& event, Duration timeout = time_infinite) -> SingleAWaiter;
} // namespace coop
