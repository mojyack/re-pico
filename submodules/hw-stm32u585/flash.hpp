#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::flash {
struct AccessControl {
    enum : u32 {
        Latency        = 0b0000'0000'0000'0000'0000'0000'0000'1111, // wait states, 4 required for 160MHz at VOS range 1
        PrefetchEnable = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        LowPowerMode   = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        PowerDownBank1 = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        PowerDownBank2 = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        SleepPowerDown = 0b0000'0000'0000'0000'0100'0000'0000'0000,
    };
};

struct Regs {
    v32 access_control; // 0x00
    // the rest (program/erase, option bytes) is not implemented yet
};
static_assert(sizeof(Regs) == 0x0 + 4);
} // namespace hw::flash

#define FLASH_REGS (*(hw::flash::Regs*)(FLASH_REGS_BASE))
