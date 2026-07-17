#pragma once
#include <noxx/vector.hpp>

#include "cohandle.hpp"
#include "event.hpp"

namespace coop {
struct Task;
struct Runner;
struct MultiAWaiter;

struct MultiEvent {
    constexpr static auto notified = usize(-1);

    struct TaskAWaiter {
        Task*         task;
        MultiAWaiter* awaiter;
    };

    noxx::Vector<TaskAWaiter> waiters;

    auto notify(usize n = 0) -> void;
};

struct [[nodiscard]] MultiAWaiter {
    Runner*     runner = nullptr;
    MultiEvent* event;
    Duration    timeout;
    bool        result;

    auto await_ready() const -> bool { return false; }
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> bool; // false on timeout
};

auto wait_for_event(MultiEvent& event, Duration timeout = time_infinite) -> MultiAWaiter;
} // namespace coop
