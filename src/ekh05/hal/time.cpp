#include <noxx/bits.hpp>

#include "../hw/m33.hpp"
#include "time.hpp"

namespace time {
namespace {
constexpr auto tick_hz       = u32(1000);
constexpr auto reload        = cpu_hz / tick_hz - 1;
constexpr auto cycles_per_us = cpu_hz / 1'000'000;

volatile auto uptime_ms = u64(0);
} // namespace

auto now() -> u64 {
    while(true) {
        const auto ms       = uptime_ms;
        const auto pending1 = SCB_REGS.int_control_state & hw::scb::IntControlState::PendSTSet;
        const auto cycles   = SYSTICK_REGS.current;
        const auto pending2 = SCB_REGS.int_control_state & hw::scb::IntControlState::PendSTSet;
        if(ms != uptime_ms || pending1 != pending2) {
            continue; // counter wrapped while reading, retry
        }
        // a set pending flag means the counter has wrapped but the handler has not run yet
        return (ms + (pending1 != 0 ? 1 : 0)) * 1000 + (reload - cycles) / cycles_per_us;
    }
}

auto delay(const u64 us) -> void {
    const auto until = now() + us;
    while(now() < until) {
    }
}

auto start_systick() -> void {
    SYSTICK_REGS.reload             = reload;
    SYSTICK_REGS.current            = 0;
    SYSTICK_REGS.control_and_status = BF(hw::systick::ControlAndStatus::Enable, 1) |
                                      BF(hw::systick::ControlAndStatus::TickInt, 1) |
                                      BF(hw::systick::ControlAndStatus::CPUClockSource, 1);
}

auto stop_systick() -> void {
    SYSTICK_REGS.control_and_status = 0;
}
} // namespace time

extern "C" auto systick_handler() -> void {
    time::uptime_ms += 1;
}
