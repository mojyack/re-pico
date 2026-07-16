#include <noxx/bits.hpp>
#include <print.hpp>

#include "hw/clocks.hpp"
#include "hw/io-bank0.hpp"
#include "hw/pll.hpp"
#include "hw/resets.hpp"
#include "hw/rom.hpp"
#include "hw/rosc.hpp"
#include "hw/sio.hpp"
#include "hw/wd.hpp"
#include "hw/xosc.hpp"
#include "system.hpp"

auto wait_for_bit(cv32& reg, const u32 mask) -> void {
    while(!(reg & mask)) {
    }
}

auto unreset(const u32 reset_num) -> void {
    RESETS_REGS_CLEAR.reset = reset_num;
    wait_for_bit(RESETS_REGS.reset_done, reset_num);
}

auto enable_led() -> void {
    unreset(resets::ResetNum::IOBank0);
    IO_BANK0_REGS.status_control[led_pin].control = BF(iobank0::GPIOControl::FuncSelect, iobank0::GPIOControlFuncSelect::SIO);
    SIO_REGS.gpio_out_en_set                      = 1 << led_pin;
}

auto led(const bool on) -> void {
    if(on) {
        SIO_REGS.gpio_out_set = 1 << led_pin;
    } else {
        SIO_REGS.gpio_out_clear = 1 << led_pin;
    }
}

auto init_system() -> void {
    // enable xosc
    XOSC_REGS.control =
        BF(xosc::Control::Enable, xosc::ControlEnable::Enable) |
        BF(xosc::Control::FreqRange, xosc::ControlFreqRange::_1p15MHz);
    wait_for_bit(XOSC_REGS.status, xosc::Status::Stable);
    // enable system pll
    unreset(resets::ResetNum::PLLSys);
    PLL_SYS_REGS.feedback_div     = 100; // VCO clock = 12MHz * 100 = 1.2GHz
    PLL_SYS_REGS_CLEAR.power_down = BF(pll::PowerDown::Core, 1) | BF(pll::PowerDown::VCO, 1);
    wait_for_bit(PLL_SYS_REGS.control_and_status, pll::ControlAndStatus::Lock);
    PLL_SYS_REGS.primary          = BF(pll::Primary::Postdiv1, 6) | BF(pll::Primary::Postdiv2, 2); // 1.2GHz / 6 / 2 = 100MHz
    PLL_SYS_REGS_CLEAR.power_down = BF(pll::PowerDown::Postdiv, 1);
    // setup clock generators
    CLOCKS_REGS_SET.clock_ref.control = BF(clocks::RefClockControl::Source, clocks::RefClockSource::XOSC);
    wait_for_bit(CLOCKS_REGS.clock_ref.selected, 1 << clocks::RefClockSource::XOSC);
    CLOCKS_REGS_SET.clock_sys.control = BF(clocks::SysClockControl::Source, clocks::SysClockSource::Aux);
    wait_for_bit(CLOCKS_REGS.clock_sys.selected, 1 << clocks::SysClockSource::Aux);
    // stop rosc
    ROSC_REGS_SET.control = BF(rosc::Control::Enable, rosc::ControlEnable::Disable);
    // enable 64-bit timer
    WATCHDOG_REGS_SET.tick = BF(wd::Tick::Cycles, 12); // 1us = 12cycles / 12MHz
    unreset(resets::ResetNum::Timer);
}

// noxx support
namespace noxx {
auto console_out(const char* ptr) -> bool {
    print_blocking(ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    rom::memcpy((u8*)dest, (u8*)src, size);
}

auto memset(void* dest, const u8 c, usize size) -> void {
    rom::memset((u8*)dest, c, size);
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
