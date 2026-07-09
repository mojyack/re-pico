#include <hal/time.hpp>
#include <noxx/assert.hpp>
#include <noxx/bits.hpp>

#include "../halow/platform.hpp"
#include "hal/gpio.hpp"
#include "hw/gpio.hpp"
#include "hw/rcc.hpp"
#include "hw/spi.hpp"

namespace {
const gpio::Line lines[] = {
    [u8(halow::Pin::Reset)] = {&GPIOE_REGS, 0},
    [u8(halow::Pin::Wake)]  = {&GPIOD_REGS, 0},
    [u8(halow::Pin::Busy)]  = {&GPIOB_REGS, 5},
    [u8(halow::Pin::Irq)]   = {&GPIOB_REGS, 15},
    [u8(halow::Pin::Cs)]    = {&GPIOB_REGS, 4},
    [u8(halow::Pin::Sck)]   = {&GPIOD_REGS, 1},
    [u8(halow::Pin::Miso)]  = {&GPIOD_REGS, 3},
    [u8(halow::Pin::Mosi)]  = {&GPIOD_REGS, 4},
};
} // namespace

// platform callbacks
namespace halow {
auto get_gpio_line(const Pin pin) -> const gpio::Line& {
    return lines[u8(pin)];
}
} // namespace halow

auto prepare_pins_for_halow() -> bool {
    constexpr auto error_value = false;

    // enable gpio pins and clocks
    RCC_REGS.ahb2_enable1 |= hw::rcc::AHB2Enable1::GPIOB | hw::rcc::AHB2Enable1::GPIOD | hw::rcc::AHB2Enable1::GPIOE;
    RCC_REGS.apb1_enable1 |= hw::rcc::APB1Enable1::SPI2;
    RCC_REGS.clock_config1 = (RCC_REGS.clock_config1 & ~hw::rcc::ClockConfig1::SPI2Source) |
                             BF(hw::rcc::ClockConfig1::SPI2Source, hw::rcc::ClockConfig1SPI2Source::SysClk);

    // spi pins
    const auto pin_sck  = lines[u8(halow::Pin::Sck)].pin;
    const auto pin_miso = lines[u8(halow::Pin::Miso)].pin;
    const auto pin_mosi = lines[u8(halow::Pin::Mosi)].pin;

    GPIOD_REGS.alt_function[0] = (GPIOD_REGS.alt_function[0] &
                                  ~(hw::gpio::alt_function_mask(pin_sck) | hw::gpio::alt_function_mask(pin_miso) | hw::gpio::alt_function_mask(pin_mosi))) |
                                 hw::gpio::alt_function(pin_sck, 5) |
                                 hw::gpio::alt_function(pin_miso, 5) |
                                 hw::gpio::alt_function(pin_mosi, 5);
    GPIOD_REGS.output_speed    = (GPIOD_REGS.output_speed &
                                  ~(hw::gpio::mode_mask(pin_sck) | hw::gpio::mode_mask(pin_miso) | hw::gpio::mode_mask(pin_mosi))) |
                                 hw::gpio::mode(pin_sck, hw::gpio::Speed::VeryHigh) |
                                 hw::gpio::mode(pin_miso, hw::gpio::Speed::VeryHigh) |
                                 hw::gpio::mode(pin_mosi, hw::gpio::Speed::VeryHigh);

    // mode 0, msb first, 8-bit frames, software cs, kernel clock / 8 = 20MHz
    SPI2_REGS.config1  = BF(hw::spi::Config1::DataSize, 8 - 1) |
                         BF(hw::spi::Config1::BaudRate, 2);
    SPI2_REGS.config2  = hw::spi::Config2::Master | hw::spi::Config2::SoftSS | hw::spi::Config2::KeepAF;
    SPI2_REGS.control2 = 0; // endless transfer
    SPI2_REGS.control1 = hw::spi::Control1::InternalSS;
    // enabling right after configuration raises a spurious mode fault, let the SS state settle
    for(auto i = 0; i < 3; i += 1) {
        time::delay(10);
        SPI2_REGS.int_clear = hw::spi::Status::ModeFault;
        SPI2_REGS.config2 |= hw::spi::Config2::Master;
        SPI2_REGS.control1 |= hw::spi::Control1::Enable;
        SPI2_REGS.control1 |= hw::spi::Control1::MasterStart;
        if(!(SPI2_REGS.status & hw::spi::Status::ModeFault)) {
            return true;
        }
    }
    ensure(false); // spi stuck in mode fault
    return false;
}
