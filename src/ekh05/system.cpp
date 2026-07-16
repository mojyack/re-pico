#include <noxx/bits.hpp>
#include <print.hpp>

#include "hal/time.hpp"
#include "hw/flash.hpp"
#include "hw/gpio.hpp"
#include "hw/pwr.hpp"
#include "hw/rcc.hpp"
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
    // PLL registers are write-protected until the PLL actually stops
    while(RCC_REGS.control & hw::rcc::Control::PLL1Ready) {
    }
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

// noxx support
namespace noxx {
auto console_out(const char* ptr) -> bool {
    print_blocking(ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    for(auto i = usize(0); i < size; i += 1) {
        ((u8*)dest)[i] = ((const u8*)src)[i];
    }
}

auto memset(void* dest, const u8 c, usize size) -> void {
    for(auto i = usize(0); i < size; i += 1) {
        ((u8*)dest)[i] = c;
    }
}

auto memcmp(const void* a, const void* b, usize size) -> int {
    for(auto i = usize(0); i < size; i += 1) {
        const auto x = ((const u8*)a)[i];
        const auto y = ((const u8*)b)[i];
        if(x != y) {
            return x < y ? -1 : 1;
        }
    }
    return 0;
}
} // namespace noxx

// coop support
namespace coop {
auto now_us() -> u64 {
    return time::now();
}

auto sleep_until(u64 time) -> void {
    while(time::now() < time) {
    }
}
} // namespace coop
