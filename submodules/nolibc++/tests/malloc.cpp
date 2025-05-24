#include <array>
#include <print>

#include "noxx/malloc.hpp"

namespace {
auto heap = std::array<std::byte, 4096>();
}

auto main() -> int {
    noxx::set_heap(heap.data(), heap.size());

    auto ptrs = std::array<void*, 8>();
    auto size = 1;
    for(auto& ptr : ptrs) {
        ptr = noxx::malloc(size);
        size += 100;
    }
    std::println("after alloc");
    noxx::dump_state();
    for(auto n : {2, 0, 4, 5, 3, 6, 1, 7}) {
        noxx::dump_state();
        noxx::free(ptrs[n]);
    }
    std::println("after free");
    noxx::dump_state();
    return 0;
}
