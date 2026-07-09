#include <hal/gpio.hpp>

#include "../hw/gpio.hpp"

namespace gpio {
struct Line {
    hw::gpio::Regs* regs;
    usize           pin;
};
} // namespace gpio
