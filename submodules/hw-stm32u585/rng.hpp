#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

// STM32U5 true random number generator (RM0456 §33)
namespace hw::rng {
struct Regs {
    v32  control; // 0x00
    v32  status;  // 0x04
    cv32 data;    // 0x08
};

struct Control {
    enum : u32 {
        RngEn    = 0b0000'0000'0000'0000'0000'0000'0000'0100, // bit 2
        ClkErrEn = 0b0000'0000'0000'0000'0000'0000'0010'0000, // bit 5, clock error detection (0 = enabled)
        ConfigLock = 0b0000'0000'0000'0000'1000'0000'0000'0000, // bit 15
    };
};

struct Status {
    enum : u32 {
        DataReady = 0b0000'0000'0000'0000'0000'0000'0000'0001, // bit 0
        ClockErr  = 0b0000'0000'0000'0000'0000'0000'0000'0010, // bit 1
        SeedErr   = 0b0000'0000'0000'0000'0000'0000'0000'0100, // bit 2
        ClkErrInt = 0b0000'0000'0000'0000'0000'0000'0010'0000, // bit 5
        SeedErrInt = 0b0000'0000'0000'0000'0000'0000'0100'0000, // bit 6
    };
};
} // namespace hw::rng

#define RNG_REGS (*(hw::rng::Regs*)(RNG_BASE))
