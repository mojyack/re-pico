#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace pads {
struct VoltageSelect {
    enum : u32 {
        _3p3 = 0,
        _1p8 = 0,
    };
};

struct GPIODrive {
    enum : u32 {
        _2mA  = 0,
        _4mA  = 0,
        _7mA  = 0,
        _12mA = 0,
    };
};

struct Control {
    enum : u32 {
        SlewRateFast   = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        SchmittTrigger = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PullDownEnable = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        PullUpEnable   = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        Drive          = 0b0000'0000'0000'0000'0000'0000'0011'0000,
        InputEnable    = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        OutputDisable  = 0b0000'0000'0000'0000'0000'0000'1000'0000,
    };
};

struct Reg {
    enum : u32 {
        MASK = 0b0000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Bank0Regs {
    v32 voltage_select;
    v32 control[32]; // gpio0..29, swclk, swd
};

struct QSPIRegs {
    v32 voltage_select;
    v32 control[6]; // sclk, sd0..3, ss
};
} // namespace pads

#define PADS_BANK0_REGS       (*(pads::Bank0Regs*)(PADS_BANK0_BASE + 0x0000))
#define PADS_BANK0_REGS_XOR   (*(pads::Bank0Regs*)(PADS_BANK0_BASE + 0x1000))
#define PADS_BANK0_REGS_SET   (*(pads::Bank0Regs*)(PADS_BANK0_BASE + 0x2000))
#define PADS_BANK0_REGS_CLEAR (*(pads::Bank0Regs*)(PADS_BANK0_BASE + 0x3000))
#define PADS_QSPI_REGS        (*(pads::QSPIRegs*)(PADS_QSPI_BASE + 0x0000))
#define PADS_QSPI_REGS_XOR    (*(pads::QSPIRegs*)(PADS_QSPI_BASE + 0x1000))
#define PADS_QSPI_REGS_SET    (*(pads::QSPIRegs*)(PADS_QSPI_BASE + 0x2000))
#define PADS_QSPI_REGS_CLEAR  (*(pads::QSPIRegs*)(PADS_QSPI_BASE + 0x3000))
