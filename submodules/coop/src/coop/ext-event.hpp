#pragma once
#include <noxx/atomic.hpp>

#include "ext-event-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
inline auto ExtEvent::notify() -> void {
    if(waiter != nullptr) {
        awaiter->runner->event_notify(*this);
        return;
    }
}

inline auto ExtAWaiter::await_ready() -> bool {
    if(event->waiter != nullptr) {
        // error, event held by another task. do not suspend
        result = EventResult::Error;
        return true;
    }

    return event->available();
}

template <CoHandleLike CoHandle>
inline auto ExtAWaiter::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*event, *this, timeout);
}

inline auto ExtAWaiter::await_resume() const -> EventResult {
    event->waiter = nullptr;
    return result;
}

inline auto wait_for_event(ExtEvent& event, const Duration timeout) -> ExtAWaiter {
    return ExtAWaiter{
        .event   = &event,
        .timeout = timeout,
    };
}
} // namespace coop
