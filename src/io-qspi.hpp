#pragma once
#include "address-map.hpp"
#include "int.hpp"
#include "io-common.hpp"

namespace ioqspi {
using GPIOStatus = ::iocommon::GPIOStatus;

// control
struct GPIOControlFuncSel {
    enum : u32 {
        XIP  = 0b0000'0000'0000'0000'0000'0000'0000'0000,
        SIO  = 0b0000'0000'0000'0000'0000'0000'0000'0101,
        NULL = 0b0000'0000'0000'0000'0000'0000'0001'1111,
    };
};

using GPIOControlOutOver = ::iocommon::GPIOControlOutOver;

using GPIOControlOutEnOver = ::iocommon::GPIOControlOutEnOver;

using GPIOControlInOver = ::iocommon::GPIOControlInOver;

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
} // namespace ioqspi

#define IOQSPI_REGS (*(ioqspi::Regs*)(IO_QSPI_BASE))
