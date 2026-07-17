#pragma once
#include <noxx/utility.hpp>

#include "runner-pre.hpp"
#include "single-event-pre.hpp"

namespace coop {
inline auto SingleEvent::notify() -> void {
    if(waiter != nullptr && waiter != (Task*)notified) {
        awaiter->runner->event_notify(*this);
    } else {
        // no waiter yet; remember the notification for the next await
        waiter = (Task*)notified;
    }
}

inline auto SingleAWaiter::await_ready() -> bool {
    // a notify() that raced ahead of the await leaves the sentinel here
    result = noxx::exchange(event->waiter, (Task*)nullptr) == (Task*)SingleEvent::notified;
    return result;
}

template <CoHandleLike CoHandle>
inline auto SingleAWaiter::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*event, *this, timeout);
}

inline auto SingleAWaiter::await_resume() const -> bool {
    return result;
}

inline auto wait_for_event(SingleEvent& event, const Duration timeout) -> SingleAWaiter {
    return SingleAWaiter{
        .event   = &event,
        .timeout = timeout,
    };
}
} // namespace coop
