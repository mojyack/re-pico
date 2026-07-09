#include <hal/time.hpp>

namespace time {
constexpr auto cpu_hz = u32(160'000'000);

auto start_systick() -> void;
auto stop_systick() -> void;
} // namespace time
