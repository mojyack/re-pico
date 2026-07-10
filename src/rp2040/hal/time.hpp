#pragma once
#include <hal/time.hpp>

namespace time {
// clocks configured by init_system(): sys = peri = 100MHz
constexpr auto clk_sys_hz  = u32(100'000'000);
constexpr auto clk_peri_hz = u32(100'000'000);
} // namespace time
