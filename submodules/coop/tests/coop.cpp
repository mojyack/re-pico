// coop primitive tests: single-event, multi-event (via mutex/lock-guard) and
// select. each test drives a fresh Runner to completion and checks the ordering
// and mutual-exclusion guarantees the primitives are supposed to provide.
#include <stdio.h>

#include <coop/generator.hpp>
#include <coop/lock-guard.hpp>
#include <coop/multi-event.hpp>
#include <coop/mutex.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/select.hpp>
#include <coop/single-event.hpp>
#include <coop/timer.hpp>

#include <noxx/assert.hpp>
#include <noxx/malloc.hpp>

namespace {
// a plain delay task usable as a select branch
auto delay_task(const u64 ms) -> coop::Async<void> {
    co_await coop::sleep_ms(ms);
}

auto test_single_event() -> bool {
    constexpr auto error_value = false;

    auto runner = coop::Runner();
    auto event  = coop::SingleEvent();
    auto got    = 0;

    ensure(runner.push_task([](coop::SingleEvent& event, int& got) -> coop::Async<void> {
        co_await event;
        got = 1;
    }(event, got)));
    ensure(runner.push_task([](coop::SingleEvent& event) -> coop::Async<void> {
        co_await coop::sleep_ms(1);
        event.notify();
    }(event)));

    ensure(runner.run());
    ensure(got == 1);
    return true;
}

// notify() before anyone awaits must still wake a later awaiter
auto test_single_event_prenotify() -> bool {
    constexpr auto error_value = false;

    auto runner = coop::Runner();
    auto event  = coop::SingleEvent();
    auto got    = 0;

    event.notify(); // fires before any await
    ensure(runner.push_task([](coop::SingleEvent& event, int& got) -> coop::Async<void> {
        co_await event;
        got = 1;
    }(event, got)));

    ensure(runner.run());
    ensure(got == 1);
    return true;
}

// three workers each read-modify-write a shared counter across a suspension
// point. without mutual exclusion they would all observe 0 and the counter
// would end at 1; with the mutex it must serialize to 3.
auto test_mutex() -> bool {
    constexpr auto error_value = false;

    auto runner  = coop::Runner();
    auto mutex   = coop::Mutex();
    auto counter = 0;

    const auto worker = [](coop::Mutex& mutex, int& counter) -> coop::Async<void> {
        auto       guard = co_await coop::LockGuard::lock(mutex);
        const auto seen  = counter;
        co_await coop::sleep_ms(1);
        counter = seen + 1;
    };
    ensure(runner.push_task(worker(mutex, counter)));
    ensure(runner.push_task(worker(mutex, counter)));
    ensure(runner.push_task(worker(mutex, counter)));

    ensure(runner.run());
    ensure(counter == 3);
    return true;
}

// multiple waiters released together by a single notify()
auto test_multi_event() -> bool {
    constexpr auto error_value = false;

    auto runner = coop::Runner();
    auto event  = coop::MultiEvent();
    auto woken  = 0;

    const auto waiter = [](coop::MultiEvent& event, int& woken) -> coop::Async<void> {
        co_await event;
        woken += 1;
    };
    ensure(runner.push_task(waiter(event, woken)));
    ensure(runner.push_task(waiter(event, woken)));
    ensure(runner.push_task(waiter(event, woken)));
    ensure(runner.push_task([](coop::MultiEvent& event) -> coop::Async<void> {
        co_await coop::sleep_ms(1); // let the waiters register first
        event.notify();             // wake all
    }(event)));

    ensure(runner.run());
    ensure(woken == 3);
    return true;
}

// select resolves to the branch that finishes first and cancels the rest
auto test_select() -> bool {
    constexpr auto error_value = false;

    auto runner = coop::Runner();
    auto winner = usize(999);

    ensure(runner.push_task([](usize& winner) -> coop::Async<void> {
        winner = co_await coop::select(delay_task(50), delay_task(1), delay_task(80));
    }(winner)));

    ensure(runner.run());
    ensure(winner == 1);
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;

    static auto heap = noxx::Array<u8, 1 << 20>();
    noxx::set_heap(heap.data, heap.size());

    ensure(test_single_event());
    ensure(test_single_event_prenotify());
    ensure(test_mutex());
    ensure(test_multi_event());
    ensure(test_select());
    printf("all tests passed\n");
    return 0;
}
