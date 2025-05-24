#pragma once
#include "int.hpp"

namespace noxx {
auto set_heap(void* ptr, usize size) -> void;
auto malloc(usize size) -> void*;
auto free(void* ptr) -> void;
} // namespace noxx

// placement new support
inline auto operator new(usize, void* ptr) -> void* {
    return ptr;
}

#ifdef NOXX_TEST
namespace noxx {
auto dump_state() -> void;
} // namespace noxx
#endif
