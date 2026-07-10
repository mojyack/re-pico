#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::dbgmcu {
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
} // namespace hw::dbgmcu

#define DBGMCU_REGS (*(hw::dbgmcu::Regs*)(DBGMCU_BASE))
