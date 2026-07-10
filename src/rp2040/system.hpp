#pragma once
#include <noxx/int.hpp>

auto wait_for_bit(cv32& reg, u32 mask) -> void;
auto unreset(u32 reset_num) -> void;
auto init_system() -> void;

// on-board led (gpio25)
constexpr auto led_pin = u32(25);
auto enable_led() -> void;
auto led(bool on) -> void;
