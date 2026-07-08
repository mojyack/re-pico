#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace systick {
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
} // namespace systick

namespace scb {
struct AppIntControlVectKey {
    enum : u32 {
        Key = 0x05FA,
    };
};

struct AppIntControl {
    enum : u32 {
        VectActive   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SysResetReq  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SysResetReqS = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        PriGroup     = 0b0000'0000'0000'0000'0000'0111'0000'0000,
        BfHfNMINS    = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        Pris         = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        Endianness   = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        VectKey      = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

struct Regs {
    cv32 cpuid;                  // 0x00
    v32  int_control_state;      // 0x04
    v32  vector_table_offset;    // 0x08
    v32  app_int_control;        // 0x0C
    v32  system_control;         // 0x10
    v32  config_control;         // 0x14
    v32  system_priority[3];     // 0x18
    v32  system_handler_control; // 0x24
    v32  fault_status;           // 0x28
    v32  hard_fault_status;      // 0x2C
};
static_assert(sizeof(Regs) == 0x2C + 4);
} // namespace scb

namespace dbgmcu {
struct IDCode {
    enum : u32 {
        DeviceID = 0b0000'0000'0000'0000'0000'1111'1111'1111, // 0x482 = STM32U575/585
        Revision = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

struct Regs {
    cv32 idcode; // 0x00
    v32  config; // 0x04
};
static_assert(sizeof(Regs) == 0x4 + 4);
} // namespace dbgmcu

#define SYSTICK_REGS (*(systick::Regs*)(PPB_BASE + 0xE010))
#define SCB_REGS     (*(scb::Regs*)(PPB_BASE + 0xED00))
#define DBGMCU_REGS  (*(dbgmcu::Regs*)(DBGMCU_BASE))
