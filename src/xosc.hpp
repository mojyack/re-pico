#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace xosc {
struct ControlFreqRange {
    enum : u32 {
        _1p15MHz = 0xaa0,
    };
};

struct ControlEnable {
    enum : u32 {
        DISABLE = 0xd1e,
        ENABLE  = 0xfab,
    };
};

struct Control {
    enum : u32 {
        FREQ_RANGE = 0b0000'0000'0000'0000'0000'1111'1111'1111,
        ENABLE     = 0b0000'0000'1111'1111'1111'0000'0000'0000,
    };
};

using StatusFreqRange = ControlFreqRange;

struct Status {
    enum : u32 {
        FREQ_RANGE = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        ENABLED    = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        BADWRITE   = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        STABLE     = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Dormant {
    enum : u32 {
        DORMANT = 0x636f6d61,
        WAKE    = 0x77616b65,
    };
};

struct StartUp {
    enum : u32 {
        DELAY = 0b0000'0000'0000'0000'0011'1111'1111'1111,
        X4    = 0b0000'0000'0001'0000'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  control;
    cv32 status;
    v32  dormant;
    v32  startup;
    cv32 reserved1[3];
    v32  count;
};
} // namespace xosc

#define XOSC_REGS (*(xosc::Regs*)(XOSC_BASE))
