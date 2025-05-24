#pragma once
#include "int.hpp"

namespace noxx {
auto set_heap(void* ptr, usize size) -> void;
auto malloc(usize size) -> void*;
auto free(void* ptr) -> void;
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
