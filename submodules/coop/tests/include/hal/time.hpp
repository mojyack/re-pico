#pragma once
#include <noxx/int.hpp>

// host-side stand-in for the firmware time base; implemented in
// tests/noxx-support.cpp against the host monotonic clock.
namespace time {
auto now() -> u64;
auto delay(u64 us) -> void;
} // namespace time
