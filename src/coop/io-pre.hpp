#pragma once

namespace coop {
struct Task;

inline volatile bool any_io_event_available = false;

struct IOEvent {
    Task*         waiter = nullptr;
    volatile bool available;
};
} // namespace coop
