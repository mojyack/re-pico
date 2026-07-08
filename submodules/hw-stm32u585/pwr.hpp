#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace pwr {
struct VoltageScaling {
    enum : u32 {
        BoostReady  = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        VOSReady    = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        VOS         = 0b0000'0000'0000'0011'0000'0000'0000'0000, // VoltageScalingVOS
        BoostEnable = 0b0000'0000'0000'0100'0000'0000'0000'0000, // EPOD booster, required for >55MHz
    };
};

struct VoltageScalingVOS {
    enum : u32 {
        Range4 = 0,
        Range3 = 1,
        Range2 = 2,
        Range1 = 3,
    };
};

struct Regs {
    v32  control1;         // 0x00
    v32  control2;         // 0x04
    v32  control3;         // 0x08
    v32  voltage_scaling;  // 0x0C
    v32  supply_monitor;   // 0x10
    v32  wakeup_control1;  // 0x14
    v32  wakeup_control2;  // 0x18
    v32  wakeup_control3;  // 0x1C
    v32  backup_control1;  // 0x20
    v32  backup_control2;  // 0x24
    v32  backup_protect;   // 0x28
    v32  ucpd;             // 0x2C
    v32  secure_config;    // 0x30
    v32  privilege_config; // 0x34
    cv32 status;           // 0x38
    cv32 supply_status;    // 0x3C
    cv32 backup_status;    // 0x40
    cv32 wakeup_status;    // 0x44
    v32  wakeup_clear;     // 0x48
    v32  apply_pull;       // 0x4C
    v32  pull_updown[18];  // 0x50 (PUCRA/PDCRA .. PUCRI/PDCRI)
};
static_assert(sizeof(Regs) == 0x94 + 4);
} // namespace pwr

#define PWR_REGS (*(pwr::Regs*)(PWR_BASE))
