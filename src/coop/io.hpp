#pragma once
#include <noxx/atomic.hpp>

#include "io-pre.hpp"
#include "runner-pre.hpp"

namespace coop {
inline auto IOEvent::notify() const -> void {
    if(waiter != nullptr) {
        any_io_event_available = true;
    }
}

inline auto IOAWaiter::await_ready() -> bool {
    result = event->waiter == nullptr;

    if(!result) {
        // error, event held by another task. do not suspend
        return true;
    }

    // waiter unknown until await_suspend() but we must set something or we may miss event->notify()
    event->waiter = (Task*)1;

    return event->available();
}

template <CoHandleLike CoHandle>
inline auto IOAWaiter::await_suspend(CoHandle caller_task) -> void {
    runner = caller_task.promise().runner;
    runner->io_wait(*event);
}

inline auto IOAWaiter::await_resume() const -> bool {
    event->waiter = nullptr;
    return result;
}

inline IOAWaiter::IOAWaiter(IOEvent& event)
    : event(&event) {
}
} // namespace coop
