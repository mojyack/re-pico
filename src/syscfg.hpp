#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace syscfg {
struct ProcConfig {
    enum : u32 {
        PROC0_HALTED     = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        PROC1_HALTED     = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PROC0_DAP_INSTID = 0b0000'1111'0000'0000'0000'0000'0000'0000,
        PROC1_DAP_INSTID = 0b1111'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct DebugForce {
    enum : u32 {
        PROC0_SWDO   = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        PROC0_SWDI   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PROC0_SWCLK  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        PROC0_ATTACH = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        PROC1_SWDO   = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        PROC1_SWDI   = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        PROC1_SWCLK  = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        PROC1_ATTACH = 0b0000'0000'0000'0000'0000'0000'1000'0000,
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

#define SYSCFG_REGS (*(syscfg::Regs*)(SYSCFG_BASE))
