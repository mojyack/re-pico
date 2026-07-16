#pragma once
#include <noxx/int.hpp>

namespace coop {
auto now_us() -> u64;
auto sleep_until(u64 time) -> void;
} // namespace coop
