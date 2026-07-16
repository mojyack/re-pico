#pragma once
#include <noxx/vector.hpp>

#include "cohandle.hpp"

namespace coop {
struct Task;
struct Runner;

struct [[nodiscard]] MultiEvent {
    Runner*             runner = nullptr;
    noxx::Vector<Task*> waiters;

    auto await_ready() const -> bool { return false; }
    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void;
    auto await_resume() const -> void {}

    auto notify(usize n = 0) -> void;
};
} // namespace coop
