#include <array>
#include <print>

#include "noxx/malloc.hpp"
#include "noxx/vector.hpp"

#include "noxx/assert.hpp"

namespace {
#define error_act return false

auto live_count = 0;

struct Tracked {
    int value = -1;

    Tracked() {
        live_count += 1;
    }

    Tracked(int value)
        : value(value) {
        live_count += 1;
    }

    Tracked(Tracked&& other)
        : value(other.value) {
        live_count += 1;
    }

    ~Tracked() {
        live_count -= 1;
    }
};

auto trivial() -> bool {
    auto vec = noxx::Vector<int>();
    ensure(vec.size() == 0);
    for(auto i = 0; i < 100; i += 1) {
        ensure(vec.append(i));
    }
    ensure(vec.size() == 100);
    for(auto i = 0; i < 100; i += 1) {
        ensure(vec[i] == i);
    }

    ensure(vec.resize(10));
    ensure(vec.size() == 10);
    const auto before_grow = vec.data();
    ensure(vec.resize(15)); // within capacity, no reallocation
    ensure(vec.data() == before_grow);
    ensure(vec[9] == 9);
    ensure(vec[14] == 0); // default constructed

    vec.clear();
    ensure(vec.size() == 0);
    ensure(vec.data() == nullptr);
    return true;
}

auto nontrivial() -> bool {
    {
        auto vec = noxx::Vector<Tracked>();
        for(auto i = 0; i < 20; i += 1) {
            ensure(vec.append(Tracked(i)));
        }
        ensure(live_count == 20);
        for(auto i = 0; i < 20; i += 1) {
            ensure(vec[i].value == i); // survived reallocations
        }
        ensure(vec.resize(5));
        ensure(live_count == 5);

        auto moved = noxx::move(vec);
        ensure(vec.size() == 0);
        ensure(moved.size() == 5);
        ensure(moved[4].value == 4);
        ensure(live_count == 5);
    }
    ensure(live_count == 0); // destructor released everything
    return true;
}

#undef error_act

auto heap = std::array<std::byte, 8192>();
} // namespace

auto main() -> int {
#define error_act return -1
    noxx::set_heap(heap.data(), heap.size());
    ensure(trivial());
    ensure(nontrivial());
    std::println("pass");
    return 0;
}
