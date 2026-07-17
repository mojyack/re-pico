#pragma once
#include "multi-event-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
inline auto MultiEvent::notify(const usize n) -> void {
    if(waiters.size() > 0) {
        waiters[0].awaiter->runner->event_notify(*this, n);
    }
}

template <CoHandleLike CoHandle>
inline auto MultiAWaiter::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*event, *this, timeout);
}

inline auto MultiAWaiter::await_resume() const -> bool {
    return result;
}

inline auto wait_for_event(MultiEvent& event, const Duration timeout) -> MultiAWaiter {
    return MultiAWaiter{
        .event   = &event,
        .timeout = timeout,
    };
}
} // namespace coop
