#pragma once
#include <noxx/bits.hpp>

namespace gpio {
struct Line;

enum class Mode : u8 {
    Input = 0,
    Output,
    Alternate,
    Analog,
};

enum class Pull : u8 {
    None = 0,
    Up,
    Down,
};

auto set(const Line& line, bool value) -> void;
auto get(const Line& line) -> bool;
auto configure(const Line& line, Mode mode) -> void;
auto configure(const Line& line, Pull pull) -> void;
} // namespace gpio
