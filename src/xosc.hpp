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
        Disable = 0xd1e,
        Enable  = 0xfab,
    };
};

struct Control {
    enum : u32 {
        FreqRange = 0b0000'0000'0000'0000'0000'1111'1111'1111,
        Enable    = 0b0000'0000'1111'1111'1111'0000'0000'0000,
    };
};

using StatusFreqRange = ControlFreqRange;

struct Status {
    enum : u32 {
        FreqRange = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        Enabled   = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        BadWrite  = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        Stable    = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Dormant {
    enum : u32 {
        Dormant_ = 0x636f6d61,
        Wake     = 0x77616b65,
    };
};

struct StartUp {
    enum : u32 {
        Delay = 0b0000'0000'0000'0000'0011'1111'1111'1111,
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
