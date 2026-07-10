#include <hal/time.hpp>

#include "runner-pre.hpp"

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

auto erase_at(noxx::Vector<Task*>& vec, const usize index) -> void {
    for(auto i = index + 1; i < vec.size(); i += 1) {
        vec[i - 1] = vec[i];
    }
    vec.resize(vec.size() - 1);
}
} // namespace impl

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
    case SuspendReason::index_of<Running>:
        ensure(running_tasks.append(&task));
        break;
    case SuspendReason::index_of<ByTimer>: {
        auto& r = reason.as<ByTimer>();
        if(r.suspend_until <= result.now) {
            reason = {};
            ensure(running_tasks.append(&task));
        } else if(r.suspend_until < result.wake) {
            result.wake = r.suspend_until;
        }
    } break;
    case SuspendReason::index_of<ByIO>: {
        auto& r = reason.as<ByIO>();
        // TODO
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
            impl::delete_task(&child);
            impl::erase_at(task.children, i);
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
    case SuspendReason::index_of<Running>: {
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
        const auto index    = impl::find_index(task);
        ensure(index < siblings.size());
        impl::delete_task(&task);
        impl::erase_at(siblings, index);
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
    const auto index = impl::find_index(child);
    ensure(index < root.children.size());
    ensure(current_task->children.append(&child));
    impl::erase_at(root.children, index);
    child.parent = current_task;
    return true;
}

auto Runner::delay(const u64 duration_us) -> void {
    current_task->suspend_reason.emplace<ByTimer>(time::now() + duration_us);
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

    auto result = GatheringResult{.now = time::now()};
    ensure(gather_resumable_tasks(root, result));

    if(running_tasks.size() > 0) {
        run_tasks(); // resume already waked tasks
        goto loop;
    }

    ensure(result.wake != u64(-1), "deadlock, no resumable task");
    while(time::now() < result.wake) {
        // idle until the next timer expires
    }
    goto loop;
}
} // namespace coop
