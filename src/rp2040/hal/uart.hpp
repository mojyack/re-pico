#pragma once
#include <noxx/int.hpp>

namespace uart {
auto init(uint baud_rate) -> void;
auto deinit() -> void;

auto uart0_handler() -> void; // rx/tx interrupt
} // namespace uart
