#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace sio {
struct FIFOStatus {
    enum : u32 {
        Valid         = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Ready         = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        WriteOverflow = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        ReadUnderflow = 0b0000'0000'0000'0000'0000'0000'0000'1000,
    };
};

struct DivControlAndStatus {
    enum : u32 {
        Ready = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Dirty = 0b0000'0000'0000'0000'0000'0000'0000'0010,
    };
};

struct InterpControl {
    enum : u32 {
        Shift       = 0b0000'0000'0000'0000'0000'0000'0001'1111,
        MaskLSB     = 0b0000'0000'0000'0000'0000'0011'1110'0000,
        MaskMSB     = 0b0000'0000'0000'0000'0111'1100'0000'0000,
        Signed      = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        CrossInput  = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        CrossResult = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        AddRaw      = 0b0000'0000'0000'0100'0000'0000'0000'0000,
        ForceMSB    = 0b0000'0000'0001'1000'0000'0000'0000'0000,
        Blend       = 0b0000'0000'0010'0000'0000'0000'0000'0000, // lane0 only
        Overflow0   = 0b0000'0000'1000'0000'0000'0000'0000'0000, // lane0 only
        Overflow1   = 0b0000'0001'0000'0000'0000'0000'0000'0000, // lane0 only
        Overflow    = 0b0000'0010'0000'0000'0000'0000'0000'0000, // lane0 only
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
    v32          gpio_out_clear;
    v32          gpio_out_xor;
    v32          gpio_out_en;
    v32          gpio_out_en_set;
    v32          gpio_out_en_clear;
    v32          gpio_out_en_xor;
    v32          gpio_hi_out;
    v32          gpio_hi_out_set;
    v32          gpio_hi_out_clear;
    v32          gpio_hi_out_xor;
    v32          gpio_hi_out_en;
    v32          gpio_hi_out_en_set;
    v32          gpio_hi_out_en_clr;
    v32          gpio_hi_out_en_xor;
    v32          fifo_status;
    v32          fifo_write;
    v32          fifo_read;
    cv32         spinlock_status;
    v32          div_udividend;
    v32          div_udivisor;
    v32          div_sdividend;
    v32          div_sdivisor;
    v32          div_quotient;
    v32          div_remainder;
    cv32         div_control_and_status;
    cv32         reserved2;
    Interpolator interp0;
    Interpolator interp1;
    v32          spinlock[32];
};
} // namespace sio

#define SIO_REGS (*(sio::Regs*)(SIO_BASE))
