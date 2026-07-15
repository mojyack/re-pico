#pragma once
#include "int.hpp"

namespace noxx {
auto console_out(const char* ptr) -> bool;
auto memcpy(void* dest, const void* src, usize size) -> void;
auto memset(void* a, u8 c, usize size) -> void;
auto memcmp(const void* a, const void* b, usize size) -> int;
} // namespace noxx
