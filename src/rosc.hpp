#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace rosc {
// control
struct ControlFreqRange {
    enum : u32 {
        LOW     = 0xfa4,
        MEDIUM  = 0xfa5,
        HIGH    = 0xfa7,
        TOOHIGH = 0xfa6,
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

// freq a
struct FreqAPassword {
    enum : u32 {
        PASS = 0x9696,
    };
};

struct FreqA {
    enum : u32 {
        DS0    = 0b0000'0000'0000'0000'0000'0000'0000'0111,
        DS1    = 0b0000'0000'0000'0000'0000'0000'0111'0000,
        DS2    = 0b0000'0000'0000'0000'0000'0111'0000'0000,
        DS3    = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        PASSWD = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

// freq b
using FreqBPassword = FreqAPassword;

struct FreqB {
    enum : u32 {
        DS4    = 0b0000'0000'0000'0000'0000'0000'0000'0111,
        DS5    = 0b0000'0000'0000'0000'0000'0000'0111'0000,
        DS6    = 0b0000'0000'0000'0000'0000'0111'0000'0000,
        DS7    = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        PASSWD = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

// dormant
struct Dormant {
    enum : u32 {
        DORMANT = 0x636f6d61,
        WAKE    = 0x77616b65,
    };
};

// phase
struct PhasePassword {
    enum : u32 {
        PASS = 0xaa,
    };
};

struct Phase {
    enum : u32 {
        SHIFT  = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        FLIP   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        ENABLE = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        PASSWD = 0b0000'0000'0000'0000'0000'1111'1111'0000,
    };
};

// status
struct Status {
    enum : u32 {
        ENABLED     = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        DIV_RUNNING = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        BADWRITE    = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        STABLE      = 0b1000'0000'0000'0000'0000'0000'0000'0000,
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

#define ROSC_REGS (*(rosc::Regs*)(ROSC_BASE))
