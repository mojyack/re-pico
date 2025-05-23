#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace rosc {
// control
struct ControlFreqRange {
    enum : u32 {
        Low     = 0xfa4,
        Medium  = 0xfa5,
        High    = 0xfa7,
        Toohigh = 0xfa6,
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

// freq a
struct FreqAPassword {
    enum : u32 {
        Password = 0x9696,
    };
};

struct FreqA {
    enum : u32 {
        DS0      = 0b0000'0000'0000'0000'0000'0000'0000'0111,
        DS1      = 0b0000'0000'0000'0000'0000'0000'0111'0000,
        DS2      = 0b0000'0000'0000'0000'0000'0111'0000'0000,
        DS3      = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        Password = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

// freq b
using FreqBPassword = FreqAPassword;

struct FreqB {
    enum : u32 {
        DS4      = 0b0000'0000'0000'0000'0000'0000'0000'0111,
        DS5      = 0b0000'0000'0000'0000'0000'0000'0111'0000,
        DS6      = 0b0000'0000'0000'0000'0000'0111'0000'0000,
        DS7      = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        Password = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

// dormant
struct Dormant {
    enum : u32 {
        Dormant_ = 0x636f6d61,
        Wake     = 0x77616b65,
    };
};

// phase
struct PhasePassword {
    enum : u32 {
        Password = 0xaa,
    };
};

struct Phase {
    enum : u32 {
        Shift  = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        Flip   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        Enable = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        Passwd = 0b0000'0000'0000'0000'0000'1111'1111'0000,
    };
};

// status
struct Status {
    enum : u32 {
        Enabled    = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        DivRunning = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        BadWrite   = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        Stable     = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  control;
    v32  freqa;
    v32  freqb;
    v32  dormant;
    v32  div;
    v32  phase;
    cv32 status;
    cv32 randombit;
    v32  count;
};
} // namespace rosc

#define ROSC_REGS       (*(rosc::Regs*)(ROSC_BASE + 0x0000))
#define ROSC_REGS_XOR   (*(rosc::Regs*)(ROSC_BASE + 0x1000))
#define ROSC_REGS_SET   (*(rosc::Regs*)(ROSC_BASE + 0x2000))
#define ROSC_REGS_CLEAR (*(rosc::Regs*)(ROSC_BASE + 0x3000))
