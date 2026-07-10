#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::scb {
struct IntControlState {
    enum : u32 {
        PendSTClr = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PendSTSet = 0b0000'0100'0000'0000'0000'0000'0000'0000,
    };
};

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
} // namespace hw::scb

#define SCB_REGS (*(hw::scb::Regs*)SCB_BASE)
