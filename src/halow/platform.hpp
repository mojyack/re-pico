#pragma once
#include <hal/gpio.hpp>

namespace halow {
enum class Pin : u8 {
    Reset = 0,
    Wake,
    Busy,
    Irq,
    Cs,
    Sck,
    Miso,
    Mosi,
};

auto get_gpio_line(Pin pin) -> const gpio::Line&;
} // namespace halow
