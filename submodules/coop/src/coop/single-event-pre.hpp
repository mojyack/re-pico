#pragma once
#include "cohandle.hpp"

namespace coop {
struct Task;
struct Runner;

struct [[nodiscard]] SingleEvent {
    constexpr static auto notified = usize(-1);

    Runner* runner = nullptr;
    Task*   waiter = nullptr;

    auto await_ready() -> bool;
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void {}

    auto notify() -> void;
};
} // namespace coop
