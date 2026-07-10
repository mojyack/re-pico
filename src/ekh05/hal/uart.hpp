#pragma once
#include <noxx/int.hpp>

namespace uart {
auto init(const uint baud_rate) -> void;
auto deinit() -> void;
auto putc(const u8 c) -> void;
auto getc() -> u8;
auto getc_timeout(const u64 timeout_us, u8& out) -> bool;

auto lpuart1_handler() -> void; // rx interrupt
} // namespace uart
