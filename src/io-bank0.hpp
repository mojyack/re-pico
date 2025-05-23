#pragma once
#include "address-map.hpp"
#include "int.hpp"
#include "io-common.hpp"

namespace iobank0 {
using GPIOStatus = ::iocommon::GPIOStatus;

// control
struct GPIOControlFuncSel {
    enum : u32 {
        SPI  = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        UART = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        I2C  = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        PWM  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SIO  = 0b0000'0000'0000'0000'0000'0000'0000'0101,
        PIO0 = 0b0000'0000'0000'0000'0000'0000'0000'0110,
        PIO1 = 0b0000'0000'0000'0000'0000'0000'0000'0111,
        GPCK = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        USB  = 0b0000'0000'0000'0000'0000'0000'0000'1001,
        NULL = 0b0000'0000'0000'0000'0000'0000'0001'1111,
    };
};

using GPIOControlOutOver = ::iocommon::GPIOControlOutOver;

using GPIOControlOutEnOver = ::iocommon::GPIOControlOutEnOver;

using GPIOControlInOver = ::iocommon::GPIOControlInOver;
// ~control

struct IRQControl {
    v32  enable[4];
    v32  force[4];
    cv32 status[4];
};

struct Regs {
    struct {
        cv32 status;
        v32  control;
    } status_control[30];
    v32        int_raw[4];
    IRQControl proc0_irq_control;
    IRQControl proc1_irq_control;
    IRQControl dormant_wake_irq_control;
};

// helpers
struct InterruptFlags {
    bool edge_high;
    bool edge_low;
    bool level_hight;
    bool level_low;
};

inline auto read_int_flag(const u8 gpio, const v32 (&arr)[4]) -> InterruptFlags {
    const auto shift = gpio % 8 * 4;
    const auto num   = (arr[gpio / 8] & (0b1111 << shift)) >> shift;
    return InterruptFlags{
        bool(num & 0b1000),
        bool(num & 0b0100),
        bool(num & 0b0010),
        bool(num & 0b0001),
    };
}

inline auto write_int_flag(const u8 gpio, v32 (&arr)[4], const InterruptFlags flags) -> void {
    const auto shift = gpio % 8 * 4;
    auto       num   = arr[gpio / 8];
    num &= ~(0b1111 << shift);
    num |= (flags.edge_high << 3 | flags.edge_low << 2 | flags.level_hight << 1 | flags.level_low << 0) << shift;
    arr[gpio / 8] = num;
}
} // namespace iobank0

#define IOBANK0_REGS (*(iobank0::Regs*)(IO_BANK0_BASE))
