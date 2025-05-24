#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace syscfg {
struct ProcConfig {
    enum : u32 {
        PROC0Halted        = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        PROC1Halted        = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PROC0DAPInstanceID = 0b0000'1111'0000'0000'0000'0000'0000'0000,
        PROC1DAPInstanceID = 0b1111'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct DebugForce {
    enum : u32 {
        PROC0SWDO   = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        PROC0SWDI   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PROC0SWCLK  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        PROC0Attach = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        PROC1SWDO   = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        PROC1SWDI   = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        PROC1SWCLK  = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        PROC1Attach = 0b0000'0000'0000'0000'0000'0000'1000'0000,
    };
};

struct MemPowerDown {
    enum : u32 {
        SRAM0 = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        SRAM1 = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SRAM2 = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SRAM3 = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        SRAM4 = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        SRAM5 = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        USB   = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        ROM   = 0b0000'0000'0000'0000'0000'0000'1000'0000,
    };
};

struct Regs {
    v32 proc0_nmi_mask;
    v32 proc1_nmi_mask;
    v32 proc_config;
    v32 proc_input_sync_bypass;    // PROC_IN_SYNC_BYPASS
    v32 proc_input_sync_bypass_hi; // PROC_IN_SYNC_BYPASS_HI
    v32 debug_force;               // DBGFORCE
    v32 mem_power_down;            // MEMPOWERDOWN
};
} // namespace syscfg

#define SYSCFG_REGS       (*(syscfg::Regs*)(SYSCFG_BASE + 0x0000))
#define SYSCFG_REGS_XOR   (*(syscfg::Regs*)(SYSCFG_BASE + 0x1000))
#define SYSCFG_REGS_SET   (*(syscfg::Regs*)(SYSCFG_BASE + 0x2000))
#define SYSCFG_REGS_CLEAR (*(syscfg::Regs*)(SYSCFG_BASE + 0x3000))
