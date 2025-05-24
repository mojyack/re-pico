#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"
#include "io-common.hpp"

namespace ioqspi {
using GPIOStatus = ::iocommon::GPIOStatus;

// control
struct GPIOControlFuncSel {
    enum : u32 {
        XIP  = 0,
        SIO  = 5,
        Null = 31,
    };
};

using GPIOControlOutOverride = ::iocommon::GPIOControlOutOverride;

using GPIOControlOutEnOverride = ::iocommon::GPIOControlOutEnOverride;

using GPIOControlInOverride = ::iocommon::GPIOControlInOverride;

using GPIOControl = ::iocommon::GPIOControl;

struct IRQControl {
    v32  enable;
    v32  force;
    cv32 status;
};

struct StatusControl {
    cv32 status;
    v32  control;
};

struct Regs {
    StatusControl qspi_sclk;
    StatusControl qspi_ss;
    StatusControl qspi_sd[4];
    v32           int_raw;
    IRQControl    proc0_irq_control;
    IRQControl    proc1_irq_control;
    IRQControl    dormant_wake_irq_control;
};

// helpers
struct InterruptFlags {
    bool edge_high;
    bool edge_low;
    bool level_hight;
    bool level_low;
};

inline auto read_int_flag(const u8 gpio, const v32& field) -> InterruptFlags {
    const auto shift = gpio % 8 * 4;
    const auto num   = field >> shift;
    return InterruptFlags{
        bool(num & 0b1000),
        bool(num & 0b0100),
        bool(num & 0b0010),
        bool(num & 0b0001),
    };
}

inline auto write_int_flag(const u8 gpio, v32& field, const InterruptFlags flags) -> void {
    const auto shift = gpio % 8 * 4;
    auto       num   = field;
    num &= ~(0b1111 << shift);
    num |= (flags.edge_high << 3 | flags.edge_low << 2 | flags.level_hight << 1 | flags.level_low << 0) << shift;
    field = num;
}
} // namespace ioqspi

#define IO_QSPI_REGS       (*(ioqspi::Regs*)(IO_QSPI_BASE + 0x0000))
#define IO_QSPI_REGS_XOR   (*(ioqspi::Regs*)(IO_QSPI_BASE + 0x1000))
#define IO_QSPI_REGS_SET   (*(ioqspi::Regs*)(IO_QSPI_BASE + 0x2000))
#define IO_QSPI_REGS_CLEAR (*(ioqspi::Regs*)(IO_QSPI_BASE + 0x3000))
