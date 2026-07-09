#pragma once
#include <noxx/int.hpp>

namespace time {
auto now() -> u64;
auto delay(u64 us) -> void;
} // namespace time
