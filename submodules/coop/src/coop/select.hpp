#pragma once
#include <noxx/array.hpp>
#include <noxx/type-traits.hpp>
#include <noxx/utility.hpp>

#include "generator.hpp"
#include "promise.hpp"
#include "runner.hpp"
#include "single-event.hpp"
#include "task-handle.hpp"

namespace coop {
// run the given tasks concurrently, resolve once the first one completes and
// cancel the rest. returns the index of the task that finished first.
template <class... T>
    requires(noxx::is_same<noxx::RemoveReference<T>, coop::Async<void>> && ...)
auto select(T&&... args) -> coop::Async<usize> {
    constexpr auto n   = sizeof...(T);
    auto           tasks         = noxx::Array<coop::Async<void>, n>{noxx::forward<T>(args)...};
    auto           handles       = noxx::Array<coop::TaskHandle, n>();
    auto           done          = coop::SingleEvent();
    auto           complete_task = usize(0);
    auto           task_template = [&](const usize index) -> coop::Async<void> {
        co_await tasks[index];
        complete_task = index;
        done.notify();
    };
    auto& runner = *co_await coop::reveal_runner();
    for(auto i = usize(0); i < n; i += 1) {
        runner.push_task(task_template(i), &handles[i]);
    }
    co_await done;
    for(auto i = usize(0); i < n; i += 1) {
        handles[i].cancel();
    }
    co_return complete_task;
}
} // namespace coop
