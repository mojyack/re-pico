#pragma once
#include <noxx/utility.hpp>

#include "runner.hpp"
#include "single-event-pre.hpp"

namespace coop {
inline auto SingleEvent::await_ready() -> bool {
    // a notify() that raced ahead of the await leaves the sentinel here
    return noxx::exchange(waiter, (Task*)nullptr) == (Task*)notified;
}

template <CoHandleLike CoHandle>
inline auto SingleEvent::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->event_wait(*this);
}

inline auto SingleEvent::notify() -> void {
    if(waiter != nullptr && waiter != (Task*)notified) {
        runner->event_notify(*this);
    } else {
        // no waiter yet; remember the notification for the next await
        waiter = (Task*)notified;
    }
}
} // namespace coop
