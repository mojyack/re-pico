// common routines between firmware and bootloader
#pragma once
#include <noxx/int.hpp>

auto init_system() -> void;
auto wait_for_bit(cv32& reg, const u32 mask) -> void;

// led
constexpr auto led_green = u32(7);  // PE7
constexpr auto led_blue  = u32(8);  // PE8
constexpr auto led_red   = u32(11); // PE11

auto enable_leds() -> void;
auto led(const u32 pin, const bool on) -> void;

// uart
auto init_uart(const uint baud_rate) -> void;
auto uart_putc(const u8 c) -> void;
auto uart_getc() -> u8;

// print
auto print(const char* str) -> void;
auto println(const char* const str) -> void;
