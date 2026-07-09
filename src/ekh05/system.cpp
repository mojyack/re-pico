#include <noxx/bits.hpp>

#include "hal/sleep.hpp"
#include "hw/flash.hpp"
#include "hw/gpio.hpp"
#include "hw/pwr.hpp"
#include "hw/rcc.hpp"
#include "hw/usart.hpp"
#include "system.hpp"

auto wait_for_bit(cv32& reg, const u32 mask) -> void {
    while(!(reg & mask)) {
    }
}

auto enable_leds() -> void {
    RCC_REGS.ahb2_enable1 |= hw::rcc::AHB2Enable1::GPIOE;
    GPIOE_REGS.mode = (GPIOE_REGS.mode &
                       ~(hw::gpio::mode_mask(led_green) | hw::gpio::mode_mask(led_blue) | hw::gpio::mode_mask(led_red))) |
                      hw::gpio::mode(led_green, hw::gpio::Mode::Output) |
                      hw::gpio::mode(led_blue, hw::gpio::Mode::Output) |
                      hw::gpio::mode(led_red, hw::gpio::Mode::Output);
}

auto led(const u32 pin, const bool on) -> void {
    GPIOE_REGS.bit_set_reset = 1 << (on ? pin : pin + 16);
}

auto init_system() -> void {
    // revert to reset clock so that PLL1 can be reconfigrued. no-op on cold boot.
    RCC_REGS.config1 = BF(hw::rcc::Config1::SysClockSource, hw::rcc::Config1SysClockSource::MSIS);
    while(FB(hw::rcc::Config1::SysClockStatus, RCC_REGS.config1) != hw::rcc::Config1SysClockSource::MSIS) {
    }
    RCC_REGS.control &= ~hw::rcc::Control::PLL1On;
    // raise core voltage to range 1 (required for 160MHz)
    RCC_REGS.ahb3_enable |= hw::rcc::AHB3Enable::PWR;
    PWR_REGS.voltage_scaling = BF(hw::pwr::VoltageScaling::VOS, hw::pwr::VoltageScalingVOS::Range1);
    wait_for_bit(PWR_REGS.voltage_scaling, hw::pwr::VoltageScaling::VOSReady);
    // 4 wait states for 160MHz at voltage range 1
    FLASH_REGS.access_control = BF(hw::flash::AccessControl::Latency, 4);
    while(FB(hw::flash::AccessControl::Latency, FLASH_REGS.access_control) != 4) {
    }
    // configure system pll input (MSIS is 4MHz at reset), which also clocks the EPOD booster
    RCC_REGS.pll1_config =
        BF(hw::rcc::PLL1Config::Source, hw::rcc::PLL1ConfigSource::MSIS) |
        BF(hw::rcc::PLL1Config::InputRange, hw::rcc::PLL1ConfigInputRange::_4to8MHz) |
        BF(hw::rcc::PLL1Config::M, 1 - 1) |
        BF(hw::rcc::PLL1Config::MBoost, 0 /*EPOD input = 4MHz*/);
    // enable the EPOD booster (required for >55MHz)
    PWR_REGS.voltage_scaling |= BF(hw::pwr::VoltageScaling::BoostEnable, 1);
    wait_for_bit(PWR_REGS.voltage_scaling, hw::pwr::VoltageScaling::BoostReady);
    RCC_REGS.pll1_dividers =
        BF(hw::rcc::PLL1Dividers::N, 80 - 1) | // VCO = 4MHz * 80 = 320MHz
        BF(hw::rcc::PLL1Dividers::P, 2 - 1) |
        BF(hw::rcc::PLL1Dividers::Q, 2 - 1) |
        BF(hw::rcc::PLL1Dividers::R, 2 - 1); // sysclk = 320MHz / 2 = 160MHz
    RCC_REGS.pll1_config |= BF(hw::rcc::PLL1Config::REnable, 1);
    RCC_REGS.control |= hw::rcc::Control::PLL1On;
    wait_for_bit(RCC_REGS.control, hw::rcc::Control::PLL1Ready);
    // switch sysclk to pll (AHB/APB dividers stay at /1)
    RCC_REGS.config1 = BF(hw::rcc::Config1::SysClockSource, hw::rcc::Config1SysClockSource::PLL1R);
    while(FB(hw::rcc::Config1::SysClockStatus, RCC_REGS.config1) != hw::rcc::Config1SysClockSource::PLL1R) {
    }
}

auto init_uart(const uint baud_rate) -> void {
    // LPUART1 console: PC0 = RX, PC1 = TX (stlink vcp), same pins main uses
    RCC_REGS.ahb2_enable1 |= hw::rcc::AHB2Enable1::GPIOC;
    RCC_REGS.apb3_enable |= hw::rcc::APB3Enable::LPUART1;
    (void)RCC_REGS.apb3_enable;
    GPIOC_REGS.alt_function[0] = (GPIOC_REGS.alt_function[0] &
                                  ~(hw::gpio::alt_function_mask(0) | hw::gpio::alt_function_mask(1))) |
                                 hw::gpio::alt_function(0, 8) |
                                 hw::gpio::alt_function(1, 8); // AF8 = LPUART1
    GPIOC_REGS.mode            = (GPIOC_REGS.mode & ~(hw::gpio::mode_mask(0) | hw::gpio::mode_mask(1))) |
                                 hw::gpio::mode(0, hw::gpio::Mode::Alternate) |
                                 hw::gpio::mode(1, hw::gpio::Mode::Alternate);
    // lpuart kernel clock = PCLK3 = 160MHz, baud = 256 * clock / brr
    LPUART1_REGS.baud_rate = (u64(256) * sys_clock + baud_rate / 2) / baud_rate;
    LPUART1_REGS.control1  = BF(hw::usart::Control1::EnableUART, 1) |
                             BF(hw::usart::Control1::EnableTX, 1) |
                             BF(hw::usart::Control1::EnableRX, 1);
}

auto uart_putc(const u8 c) -> void {
    wait_for_bit(LPUART1_REGS.status, hw::usart::Status::TXEmpty);
    LPUART1_REGS.transmit_data = c;
}

auto uart_getc() -> u8 {
loop:
    const auto st = LPUART1_REGS.status;
    if(st & hw::usart::Status::OverrunError) {
        LPUART1_REGS.int_clear = hw::usart::Status::OverrunError;
    }
    if(st & hw::usart::Status::RXNotEmpty) {
        return u8(LPUART1_REGS.receive_data);
    }
    goto loop;
}

auto print(const char* str) -> void {
    while(*str != '\0') {
        uart_putc(u8(*str));
        str += 1;
    }
}

auto println(const char* const str) -> void {
    print(str);
    print("\r\n");
}

// noxx support
namespace noxx {
auto console_out(const char* ptr) -> bool {
    print(ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    for(auto i = usize(0); i < size; i += 1) {
        ((u8*)dest)[i] = ((const u8*)src)[i];
    }
}
} // namespace noxx
