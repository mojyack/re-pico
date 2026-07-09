#pragma once
#include "runner.hpp"

namespace coop {
struct [[nodiscard]] TimeAwaiter {
    u64 duration_us;

    auto await_ready() const -> bool {
        return false;
    }

    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) const -> void {
        caller_task.promise().runner->delay(duration_us);
    }

    auto await_resume() const -> void {
    }
};

constexpr auto sleep_us(const u64 us) -> TimeAwaiter {
    return TimeAwaiter{us};
}

constexpr auto sleep_ms(const u64 ms) -> TimeAwaiter {
    return TimeAwaiter{ms * 1000};
}
} // namespace coop
