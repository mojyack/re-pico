#pragma once
#include <hal/time.hpp>
#include <noxx/malloc.hpp>
#include <noxx/optional.hpp>
#include <noxx/type-traits.hpp>
#include <noxx/utility.hpp>

#include "promise-pre.hpp"
#include "runner-pre.hpp"

#include <noxx/assert.hpp>

namespace coop {
namespace impl {
auto new_task(Task src) -> Task*;
auto delete_task(Task* task) -> void;
auto find_index(Task& child) -> usize;
auto erase_at(noxx::Vector<Task*>& vec, usize index) -> void;
} // namespace impl

template <CoHandleLike CoHandle>
inline auto Runner::push_task(const bool independent, const bool transfer_handle, CoHandle& handle, TaskHandle* const user_handle, const usize objective_of) -> bool {
    constexpr auto error_value = false;

    handle.promise().runner = this;

    auto& parent = independent ? root : *current_task;
    // transfer cohandle to runner if independent
    // since child task may live longer than this generator
    const auto task = impl::new_task(Task{
        .handle       = transfer_handle ? std::coroutine_handle<>(noxx::exchange(handle, nullptr)) : std::coroutine_handle<>(handle),
        .parent       = &parent,
        .user_handle  = user_handle,
        .objective_of = objective_of,
        .handle_owned = transfer_handle,
    });
    ensure(task != nullptr);
    ensure(parent.children.append(task));

    if(user_handle != nullptr) {
        *user_handle = TaskHandle{.task = task, .runner = this, .destroyed = false};
    }
    return true;
}

template <CoGeneratorLike Generator>
auto Runner::push_task(Generator generator, TaskHandle* const user_handle) -> bool {
    return push_task(true, true, generator.handle, user_handle, 0);
}

template <CoGeneratorLike Generator>
auto Runner::push_dependent_task(Generator generator) -> bool {
    return push_task(false, true, generator.handle, nullptr, 0);
}

template <CoGeneratorLike Generator>
auto Runner::await(Generator generator) -> decltype(auto) {
    constexpr auto with_ret = PromiseWithRetValue<decltype(Generator::handle.promise())>;

    const auto pushed = push_task(false, false, generator.handle, nullptr, objective_task_finished.size());
    if constexpr(with_ret) {
        using Opt = noxx::Optional<noxx::RemoveReference<decltype(generator.await_resume())>>;
        if(!pushed) {
            return Opt();
        }
        current_task->suspend_reason = {.kind = SuspendReason::Kind::Awaiting};
        const auto done              = run();
        current_task->suspend_reason = {};
        return !done || current_task->zombie ? Opt() : Opt(generator.await_resume());
    } else {
        if(!pushed) {
            return false;
        }
        current_task->suspend_reason = {.kind = SuspendReason::Kind::Awaiting};
        const auto done              = run();
        current_task->suspend_reason = {};
        return done && !current_task->zombie;
    }
}
} // namespace coop

#include <noxx/assert.hpp>
