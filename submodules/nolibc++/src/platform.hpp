#pragma once
#include "int.hpp"

namespace noxx {
auto console_out(const char* ptr) -> bool;
auto memcpy(void* dest, const void* src, usize size) -> void;
} // namespace noxx
