#pragma once
#include <noxx/variant.hpp>
#include <noxx/vector.hpp>

#include "cohandle.hpp"
#include "generator-pre.hpp"
#include "task-handle-pre.hpp"

namespace coop {
struct IOEvent;

struct ByTimer {
    u64 suspend_until = 0;
};

struct ByIO {
    IOEvent* event;
};

struct ByAwaiting {
};

using SuspendReason = noxx::Variant<ByTimer, ByIO, ByAwaiting>;

struct Task {
    std::coroutine_handle<> handle;
    Task*                   parent;
    TaskHandle*             user_handle = nullptr;
    noxx::Vector<Task*>     children;
    usize                   objective_of = 0;
    SuspendReason           suspend_reason;
    bool                    handle_owned = true;
    bool                    zombie       = false;
};

struct Runner {
    // private
    struct GatheringResult {
        u64  now;
        u64  wake    = u64(-1);
        bool poll_io = false;
    };

    Task                root;
    Task*               current_task = &root;
    usize               loop_count   = 0;
    noxx::Vector<Task*> running_tasks;
    noxx::Vector<bool>  objective_task_finished;

    // private
    auto gather_resumable_tasks(Task& task, GatheringResult& result) -> bool;
    auto run_tasks() -> void;

    // coop internal
    template <CoHandleLike CoHandle>
    auto push_task(bool independent, bool transfer_handle, CoHandle& handle, TaskHandle* user_handle, usize objective_of) -> bool;
    auto destroy_task(Task& task) -> bool;
    auto remove_task(Task& task) -> bool;

    // for awaiters
    auto join(TaskHandle& handle) -> bool;
    auto delay(u64 duration_us) -> void;
    auto io_wait(IOEvent& event) -> void;

    // public
    template <CoGeneratorLike Generator>
    auto push_task(Generator generator, TaskHandle* user_handle = nullptr) -> bool;
    template <CoGeneratorLike Generator>
    auto push_dependent_task(Generator generator) -> bool;
    template <CoGeneratorLike Generator>
    auto await(Generator generator) -> decltype(auto);
    auto cancel_task(TaskHandle& handle) -> bool;
    auto run() -> bool;
};

struct RunnerGetter {
    Runner* runner;

    auto await_ready() const -> bool {
        return false;
    }

    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle caller_task) -> void {
        runner = caller_task.promise().runner;
    }

    auto await_resume() const -> Runner* {
        return runner;
    }
};

using reveal_runner = RunnerGetter;

struct NopAwaiter {
    auto await_ready() const -> bool {
        return false;
    }

    template <CoHandleLike CoHandle>
    auto await_suspend(CoHandle) -> void {
    }

    auto await_resume() const -> void {
    }
};

using yield = NopAwaiter;
} // namespace coop
