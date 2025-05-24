#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace psm {
struct Blocks {
    enum : u32 {
        ROSC             = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        XOSC             = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        Clocks           = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        Resets           = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        BusFabric        = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        ROM              = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        SRAM0            = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        SRAM1            = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        SRAM2            = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        SRAM3            = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        SRAM4            = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        SRAM5            = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        XIP              = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        VRegAndChipReset = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        SIO              = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        Proc0            = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        Proc1            = 0b0000'0000'0000'0001'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  force_on;  // FRCE_ON
    v32  force_off; // FRCE_OFF
    v32  wdsel;
    cv32 done;
};
} // namespace psm

#define PSM_REGS       (*(psm::Regs*)(PSM_BASE + 0x0000))
#define PSM_REGS_XOR   (*(psm::Regs*)(PSM_BASE + 0x1000))
#define PSM_REGS_SET   (*(psm::Regs*)(PSM_BASE + 0x2000))
#define PSM_REGS_CLEAR (*(psm::Regs*)(PSM_BASE + 0x3000))
