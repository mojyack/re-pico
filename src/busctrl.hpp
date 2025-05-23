#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace busctrl {
struct BusPriority {
    enum : u32 {
        Proc0    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Proc1    = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        DMARead  = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        DMAWrite = 0b0000'0000'0000'0000'0001'0000'0000'0000,
    };
};

struct PerformanceCounterSelect {
    enum : u32 {
        APBContested      = 0x00,
        APB               = 0x01,
        FastPeriContested = 0x02,
        FastPeri          = 0x03,
        SRAM5Contested    = 0x04,
        SRAM5             = 0x05,
        SRAM4Contested    = 0x06,
        SRAM4             = 0x07,
        SRAM3Contested    = 0x08,
        SRAM3             = 0x09,
        SRAM2Contested    = 0x0a,
        SRAM2             = 0x0b,
        SRAM1Contested    = 0x0c,
        SRAM1             = 0x0d,
        SRAM0Contested    = 0x0e,
        SRAM0             = 0x0f,
        XIPMainContested  = 0x10,
        XIPMain           = 0x11,
        ROMContested      = 0x12,
        ROM               = 0x13,
    };
};

struct Regs {
    v32  bus_priority;
    cv32 bus_priority_ack;
    struct {
        v32 control;
        v32 select;
    } performance_counter[4];
};
} // namespace busctrl

#define BUSCTRL_REGS       (*(busctrl::Regs*)(BUSCTRL_BASE + 0x0000))
#define BUSCTRL_REGS_XOR   (*(busctrl::Regs*)(BUSCTRL_BASE + 0x1000))
#define BUSCTRL_REGS_SET   (*(busctrl::Regs*)(BUSCTRL_BASE + 0x2000))
#define BUSCTRL_REGS_CLEAR (*(busctrl::Regs*)(BUSCTRL_BASE + 0x3000))
