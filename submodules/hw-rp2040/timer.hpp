#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace timer {
struct DebugPause {
    enum : u32 {
        Debug0 = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        Debug1 = 0b0000'0000'0000'0000'0000'0000'0000'0100,
    };
};

struct Regs {
    v32  time_high_write;  // TIMEHW
    v32  time_low_write;   // TIMELW
    cv32 time_high_read;   // TIMEHR
    cv32 time_low_read;    // TIMELR
    v32  alarm[4];         // ALARM0..3
    v32  armed;            // ARMED
    cv32 time_raw_high;    // TIMERAWH
    cv32 time_raw_low;     // TIMERAWL
    v32  debug_pause;      // DBGPAUSE
    v32  pause;            // PAUSE
    v32  raw_interrupts;   // INTR
    v32  interrupt_enable; // INTE
    v32  interrupt_force;  // INTF
    cv32 interrupt_status; // INTS
};
} // namespace timer

#define TIMER_REGS       (*(timer::Regs*)(TIMER_BASE + 0x0000))
#define TIMER_REGS_XOR   (*(timer::Regs*)(TIMER_BASE + 0x1000))
#define TIMER_REGS_SET   (*(timer::Regs*)(TIMER_BASE + 0x2000))
#define TIMER_REGS_CLEAR (*(timer::Regs*)(TIMER_BASE + 0x3000))
