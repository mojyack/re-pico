#include "gpio.hpp"

namespace gpio {
auto set(const Line& line, const bool value) -> void {
    line.regs->bit_set_reset = 1 << (line.pin + (value ? 0 : 16));
}

auto get(const Line& line) -> bool {
    return line.regs->input_data & (1 << line.pin);
}

auto configure(const Line& line, const Mode mode) -> void {
    // gpio::Mode matches to hw::gpio::Mode, no enum convesion
    line.regs->mode = (line.regs->mode & ~hw::gpio::mode_mask(line.pin)) | hw::gpio::mode(line.pin, u32(mode));
}

auto configure(const Line& line, Pull pull) -> void {
    // halow::Pull matches to hw::gpio::Pull, no enum convesion
    line.regs->pull = (line.regs->pull & ~hw::gpio::mode_mask(line.pin)) | hw::gpio::mode(line.pin, u32(pull));
}
} // namespace gpio
