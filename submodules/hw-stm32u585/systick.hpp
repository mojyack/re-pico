#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::systick {
struct ControlAndStatus {
    enum : u32 {
        Enable         = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        TickInt        = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        CPUClockSource = 0b0000'0000'0000'0000'0000'0000'0000'0100, // 1 = CPU clock, 0 = CPU clock / 8
        CountFlag      = 0b0000'0000'0000'0001'0000'0000'0000'0000, // cleared on read
    };
};

struct Regs {
    v32  control_and_status; // 0x00
    v32  reload;             // 0x04 24-bit
    v32  current;            // 0x08 write clears
    cv32 calibration;        // 0x0C
};
static_assert(sizeof(Regs) == 0xC + 4);
} // namespace hw::systick

#define SYSTICK_REGS (*(hw::systick::Regs*)SYSTICK_BASE)
