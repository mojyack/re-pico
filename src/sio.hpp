#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace sio {
struct FIFOStatus {
    enum : u32 {
        VLD = 0b0000,
        RDY = 0b0010,
        WOF = 0b0100,
        ROE = 0b1000,
    };
};

struct DivControlStatus {
    enum : u32 {
        READY = 0b0000,
        DIRTY = 0b0010,
    };
};

struct InterpControl {
    enum : u32 {
        SHIFT        = 0b0000'0000'0000'0000'0000'0000'0001'1111,
        MASK_LSB     = 0b0000'0000'0000'0000'0000'0011'1110'0000,
        MASK_MSB     = 0b0000'0000'0000'0000'0111'1100'0000'0000,
        SIGNED       = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        CROSS_INPUT  = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        CROSS_RESULT = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        ADD_RAW      = 0b0000'0000'0000'0100'0000'0000'0000'0000,
        FORCE_MSB    = 0b0000'0000'0001'1000'0000'0000'0000'0000,
        BLEND        = 0b0000'0000'0010'0000'0000'0000'0000'0000, // lane0 only
        OVERF0       = 0b0000'0000'1000'0000'0000'0000'0000'0000, // lane0 only
        OVERF1       = 0b0000'0001'0000'0000'0000'0000'0000'0000, // lane0 only
        OVERF        = 0b0000'0010'0000'0000'0000'0000'0000'0000, // lane0 only
    };
};

struct Interpolator {
    v32 accum0;
    v32 accum1;
    v32 base0;
    v32 base1;
    v32 base2;
    v32 pop_lane0;
    v32 pop_lane1;
    v32 pop_full;
    v32 peek_lane0;
    v32 peek_lane1;
    v32 peek_full;
    v32 ctrl_lane0;
    v32 ctrl_lane1;
    v32 accum0_add;
    v32 accum1_add;
    v32 base_1and0;
};

struct Regs {
    cv32         cpuid;
    cv32         gpio_in;
    cv32         gpio_hi_in;
    cv32         reserved1;
    v32          gpio_out;
    v32          gpio_out_set;
    v32          gpio_out_clr;
    v32          gpio_out_xor;
    v32          gpio_oe; // out enable
    v32          gpio_oe_set;
    v32          gpio_oe_clr;
    v32          gpio_oe_xor;
    v32          gpio_hi_out;
    v32          gpio_hi_out_set;
    v32          gpio_hi_out_clr;
    v32          gpio_hi_out_xor;
    v32          gpio_hi_oe;
    v32          gpio_hi_oe_set;
    v32          gpio_hi_oe_clr;
    v32          gpio_hi_oe_xor;
    v32          fifo_st; // status
    v32          fifo_wr; // write
    v32          fifo_rd; //  read
    cv32         spinlock_st;
    v32          div_udividend;
    v32          div_udivisor;
    v32          div_sdividend;
    v32          div_sdivisor;
    v32          div_quotient;
    v32          div_remainder;
    cv32         div_csr; // control and status
    cv32         reserved2;
    Interpolator interp0;
    Interpolator interp1;
    v32          spinlock[32];
};
} // namespace sio

#define SIO_REGS (*(sio::Regs*)(SIO_BASE))
