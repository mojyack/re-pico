#pragma once
#include "int.hpp"

namespace noxx {
auto set_heap(void* ptr, usize size) -> void;
auto malloc(usize size) -> void*;
auto free(void* ptr) -> void;

struct HeapStats {
    usize used;         // usable bytes in allocated chunks
    usize free;         // usable bytes in free chunks
    usize used_chunks;  // number of allocated chunks
    usize free_chunks;  // number of free chunks
    usize largest_free; // size of the largest free chunk
};

auto heap_stats() -> HeapStats;

// walk every chunk in allocation order, invoking callback for each
auto heap_walk(void* data, void (*callback)(void* data, const void* addr, usize size, bool is_free)) -> void;
} // namespace noxx

#if !defined(NOXX_TEST)
// placement new support
inline auto operator new(usize, void* ptr) noexcept -> void* {
    return ptr;
}
#endif

#if defined(NOXX_TEST)
namespace noxx {
auto dump_state() -> void;
} // namespace noxx
#endif
