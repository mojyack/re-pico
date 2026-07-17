#include "ext-event-pre.hpp"
#include "multi-event-pre.hpp"
#include "platform.hpp"
#include "runner-pre.hpp"
#include "single-event-pre.hpp"

#include <noxx/algorithm.hpp>
#include <noxx/assert.hpp>

namespace coop {
namespace impl {
auto new_task(Task src) -> Task* {
    const auto ptr = (Task*)noxx::malloc(sizeof(Task));
    if(ptr != nullptr) {
        new(ptr) Task(noxx::move(src));
    }
    return ptr;
}
} // namespace impl

namespace {
auto delete_task(Task* const task) -> void {
    task->~Task();
    noxx::free(task);
}

auto find_index(Task& child) -> usize {
    auto& siblings = child.parent->children;
    for(auto i = usize(0); i < siblings.size(); i += 1) {
        if(siblings[i] == &child) {
            return i;
        }
    }
    return siblings.size();
}

template <class T>
auto erase_at(noxx::Vector<T>& vec, const usize index) -> void {
    for(auto i = index + 1; i < vec.size(); i += 1) {
        vec[i - 1] = noxx::move(vec[i]);
    }
    vec.resize(vec.size() - 1);
}

auto timeout_to_deadline(const Duration timeout) -> TimePoint {
    return timeout == time_infinite ? time_infinite : now_us() + timeout;
}
} // namespace

auto Runner::gather_resumable_tasks(Task& task, GatheringResult& result) -> bool {
    constexpr auto error_value = false;

    if(task.children.size() > 0) {
        for(auto i = usize(0); i < task.children.size(); i += 1) {
            ensure(gather_resumable_tasks(*task.children[i], result));
        }
        return true;
    }

    auto& reason = task.suspend_reason;
    switch(reason.get_index()) {
    case SuspendReason::invalid_index:
        ensure(running_tasks.append(&task));
        break;
    case SuspendReason::index_of<ByTimer>: {
        auto& r = reason.as<ByTimer>();
        if(r.suspend_until <= result.now) {
            reason = {};
            ensure(running_tasks.append(&task));
        } else {
            result.wake = noxx::min(result.wake, r.suspend_until);
        }
    } break;
    case SuspendReason::index_of<ByExtEvent>: {
        auto& r = reason.as<ByExtEvent>();

        const auto avail = r.event->available();
        const auto timed = r.deadline <= result.now;
        if(avail || timed) {
            r.event->waiter          = nullptr;
            r.event->awaiter->result = avail ? EventResult::Ok : EventResult::TimedOut;
            reason                   = {};
            ensure(running_tasks.append(&task));
        } else {
            if(r.deadline != time_infinite) {
                result.wake = noxx::min(result.wake, r.deadline);
            }
            result.poll_io = true;
        }
    } break;
    case SuspendReason::index_of<BySingleEvent>: {
        auto& r = reason.as<BySingleEvent>();

        const auto timed = r.deadline <= result.now;
        if(timed) {
            r.event->waiter->suspend_reason = {};
            r.event->waiter                 = nullptr;
            r.event->awaiter->result        = false;
            ensure(running_tasks.append(&task));
        } else if(r.deadline != time_infinite) {
            result.wake = noxx::min(result.wake, r.deadline);
        }
    } break;
    case SuspendReason::index_of<ByMultiEvent>: {
        auto& r = reason.as<ByMultiEvent>();

        const auto timed = r.deadline <= result.now;
        if(timed) {
            // remove this task from waiters list
            auto& waiters = r.event->waiters;
            for(auto i = 0uz; i < waiters.size(); i += 1) {
                if(waiters[i].task == &task) {
                    waiters[i].task->suspend_reason = {};
                    waiters[i].awaiter->result      = false;
                    erase_at(waiters, i);
                    break;
                }
            }
            ensure(running_tasks.append(&task));
        } else if(r.deadline != time_infinite) {
            result.wake = noxx::min(result.wake, r.deadline);
        }
    } break;
    case SuspendReason::index_of<ByAwaiting>:
        break;
    }
    return true;
}

auto Runner::run_tasks() -> void {
    const auto orig_loop_count = loop_count;
    for(auto i = usize(0); i < running_tasks.size(); i += 1) {
        if(running_tasks[i] == nullptr) {
            continue; // destroyed
        }
        auto& task   = *noxx::exchange(running_tasks[i], nullptr);
        current_task = &task;
        task.handle.resume();
        if(task.handle.done() || task.zombie) {
            remove_task(task);
        }
        if(orig_loop_count != loop_count) {
            // run() called inside the handle.resume()
            // this loop is obsolete
            current_task = &task;
            break;
        }
    }
    running_tasks.resize(0);
}

auto Runner::destroy_task(Task& task) -> bool {
    // destroy children recursively
    for(auto i = usize(0); i < task.children.size();) {
        auto& child = *task.children[i];
        if(destroy_task(child)) {
            delete_task(&child);
            erase_at(task.children, i);
        } else {
            i += 1;
        }
    }
    if(objective_task_finished.size() > 0 && !task.zombie /*do not notify twice*/) {
        objective_task_finished[task.objective_of] = true;
    }
    if(task.suspend_reason.get_index() == SuspendReason::index_of<ByAwaiting> || task.children.size() > 0) {
        // cannot destroy it for now
        task.zombie = true;
        return false;
    }

    switch(task.suspend_reason.get_index()) {
    case SuspendReason::invalid_index: {
        if(&task == current_task) {
            // called from run_tasks
            break;
        }
        // called from cancel_task
        // we have to prevent the target task from being resumed
        for(auto i = usize(0); i < running_tasks.size(); i += 1) {
            if(running_tasks[i] == &task) {
                running_tasks[i] = nullptr; // remove target task from resume queue
                break;
            }
        }
    } break;
    case SuspendReason::index_of<ByExtEvent>: {
        auto& r         = task.suspend_reason.as<ByExtEvent>();
        r.event->waiter = nullptr;
    } break;
    case SuspendReason::index_of<BySingleEvent>: {
        auto& r         = task.suspend_reason.as<BySingleEvent>();
        r.event->waiter = nullptr;
    } break;
    case SuspendReason::index_of<ByMultiEvent>: {
        auto& waiters = task.suspend_reason.as<ByMultiEvent>().event->waiters;
        for(auto i = usize(0); i < waiters.size(); i += 1) {
            if(waiters[i].task == &task) {
                erase_at(waiters, i);
                break;
            }
        }
    } break;
    }

    if(task.user_handle != nullptr) {
        task.user_handle->task      = nullptr;
        task.user_handle->destroyed = true;
    }
    if(task.handle_owned) {
        // destroy() might call Runner::await() in destructor
        // but this marks as awaiting only the caller task of this function,
        // as current_task is not updated.
        // mark this task as awaiting to prevent scheduler to pick this up.
        task.suspend_reason.emplace<ByAwaiting>();
        task.handle.destroy();
    }

    return true;
}

auto Runner::remove_task(Task& task) -> bool {
    constexpr auto error_value = false;

    if(destroy_task(task)) {
        // find index after destroy, siblings may have changed during handle.destroy()
        auto&      siblings = task.parent->children;
        const auto index    = find_index(task);
        ensure(index < siblings.size());
        delete_task(&task);
        erase_at(siblings, index);
    }
    return true;
}

auto Runner::cancel_task(TaskHandle& handle) -> bool {
    if(handle.task == nullptr || handle.destroyed) {
        return true;
    }

    return remove_task(*handle.task);
}

auto Runner::join(TaskHandle& handle) -> bool {
    constexpr auto error_value = false;

    if(handle.task == nullptr) {
        return true;
    }

    auto& child = *handle.task;
    ensure(child.parent == &root, "tried to steal child task from another task");

    // transfer task object
    const auto index = find_index(child);
    ensure(index < root.children.size());
    ensure(current_task->children.append(&child));
    erase_at(root.children, index);
    child.parent = current_task;
    return true;
}

auto Runner::delay(const u64 duration_us) -> void {
    current_task->suspend_reason.emplace<ByTimer>(now_us() + duration_us);
}

auto Runner::event_wait(ExtEvent& event, ExtAWaiter& awaiter, const Duration timeout) -> void {
    event.waiter  = current_task;
    event.awaiter = &awaiter;
    current_task->suspend_reason.emplace<ByExtEvent>(&event, timeout_to_deadline(timeout));
}

auto Runner::event_notify(ExtEvent& event) -> void {
    // may be called from isr, do not rely on current_task
    if(event.waiter != nullptr) {
        event.awaiter->result   = EventResult::Ok;
        any_ext_event_available = true;
    }
}

auto Runner::event_wait(SingleEvent& event, SingleAWaiter& awaiter, const Duration timeout) -> void {
    current_task->suspend_reason.emplace<BySingleEvent>(&event, timeout_to_deadline(timeout));
    event.waiter  = current_task;
    event.awaiter = &awaiter;
}

auto Runner::event_notify(SingleEvent& event) -> void {
    event.waiter->suspend_reason = {};
    event.waiter                 = nullptr;
    event.awaiter->result        = true;
}

auto Runner::event_wait(MultiEvent& event, MultiAWaiter& awaiter, const Duration timeout) -> void {
    current_task->suspend_reason.emplace<ByMultiEvent>(&event, timeout_to_deadline(timeout));
    event.waiters.append({current_task, &awaiter});
}

auto Runner::event_notify(MultiEvent& event, usize n) -> void {
    auto& waiters = event.waiters;
    n             = (n == 0 || n > waiters.size()) ? waiters.size() : n;
    for(auto i = usize(0); i < n; i += 1) {
        const auto [task, awaiter] = waiters[i];
        task->suspend_reason       = {};
        awaiter->result            = true;
    }
    for(auto i = n; i < waiters.size(); i += 1) {
        waiters[i - n] = waiters[i];
    }
    waiters.resize(waiters.size() - n);
}

auto Runner::run() -> bool {
    constexpr auto error_value = false;

    const auto orig_current_task = current_task;
    ensure(objective_task_finished.append(false));

loop:
    if(objective_task_finished.size() == 1 ? /*root loop*/ root.children.size() == 0 : /*derived loop*/ objective_task_finished[objective_task_finished.size() - 1]) {
        current_task = orig_current_task;
        objective_task_finished.resize(objective_task_finished.size() - 1);
        return true;
    }

    loop_count += 1;

    if(running_tasks.size() > 0) {
        run_tasks(); // resume previously gathered tasks inherited from parent loop
        goto loop;
    }

    any_ext_event_available = false;

    auto result = GatheringResult{.now = now_us()};
    ensure(gather_resumable_tasks(root, result));

    if(running_tasks.size() > 0) {
        run_tasks(); // resume already waked tasks
        goto loop;
    }

    if(result.poll_io) {
        while(now_us() < result.wake && !any_ext_event_available) {
            // idle until the next timer expires or io events
        }
    } else {
        ensure(result.wake != u64(-1), "deadlock, no resumable task");
        sleep_until(result.wake);
    }
    goto loop;
}
} // namespace coop
