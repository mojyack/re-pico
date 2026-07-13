#include <array>
#include <print>

#include "noxx/unique-ptr.hpp"

#include "noxx/assert.hpp"

namespace {
constexpr auto error_value = false;

auto heap = std::array<std::byte, 4096>();

auto default_deleter() -> bool {
    auto p = noxx::make_unique<int>(42);
    ensure(p);
    ensure(*p == 42);

    auto q = noxx::move(p);
    ensure(!p);
    ensure(q && *q == 42);

    q.reset();
    ensure(!q);
    return true;
}

auto array_deleter() -> bool {
    auto a = noxx::make_unique_array<int>(4);
    ensure(a);
    for(auto i = usize(0); i < 4; i += 1) {
        a[i] = int(i);
    }
    ensure(a[3] == 3);

    auto b = noxx::move(a);
    ensure(!a);
    ensure(b && b[3] == 3);
    return true;
}

// counts how many pointers a stateful deleter was asked to free
struct CountingDelete {
    int* count;

    auto operator()(int* const ptr) const -> void {
        *count += 1;
        noxx::free(ptr);
    }
};

auto custom_deleter() -> bool {
    auto freed = 0;
    {
        const auto raw = (int*)noxx::malloc(sizeof(int));
        new(raw) int(7);
        auto p = noxx::UniquePtr<int, CountingDelete>(raw, CountingDelete{&freed});
        ensure(*p == 7);
        ensure(freed == 0);

        // moving must not invoke the deleter
        auto q = noxx::move(p);
        ensure(!p);
        ensure(freed == 0);
    }
    // exactly one free on scope exit
    ensure(freed == 1);
    return true;
}

auto release_test() -> bool {
    auto       freed = 0;
    const auto raw   = (int*)noxx::malloc(sizeof(int));
    new(raw) int(9);
    {
        auto p = noxx::UniquePtr<int, CountingDelete>(raw, CountingDelete{&freed});
        ensure(p.release() == raw);
        ensure(!p);
    }
    // released pointer must not be freed by the UniquePtr
    ensure(freed == 0);
    noxx::free(raw);
    return true;
}

// deleter that runs a free function instead of ~T + free
int void_free_hits = 0;

auto void_free(int* const ptr) -> void {
    void_free_hits += 1;
    noxx::free(ptr);
}

struct FnDelete {
    void (*fn)(int*);

    auto operator()(int* const ptr) const -> void {
        fn(ptr);
    }
};

auto function_deleter() -> bool {
    {
        const auto raw = (int*)noxx::malloc(sizeof(int));
        new(raw) int(1);
        auto p = noxx::UniquePtr<int, FnDelete>(raw, FnDelete{void_free});
        ensure(p.get_deleter().fn == void_free);
    }
    ensure(void_free_hits == 1);
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = -1;

    noxx::set_heap(heap.data(), heap.size());

    ensure(default_deleter());
    ensure(array_deleter());
    ensure(custom_deleter());
    ensure(release_test());
    ensure(function_deleter());
    std::println("pass");
    return 0;
}
