#pragma once
#include "int.hpp"

namespace iocommon {
struct GPIOStatus {
    enum : u32 {
        OutFromPeri   = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        OutToPad      = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        OutEnFromPeri = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        OutEnToPad    = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        InFromPad     = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        InToPeri      = 0b0000'0000'0000'1000'0000'0000'0000'0000,
        IRQFromPad    = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        IRQToProc     = 0b0000'0100'0000'0000'0000'0000'0000'0000,
    };
};

struct GPIOControlOutOverride {
    enum : u32 {
        Normal = 0x0,
        Invert = 0x1,
        Low    = 0x2,
        High   = 0x3,
    };
};

struct GPIOControlOutEnOverride {
    enum : u32 {
        Normal  = 0x0,
        Invert  = 0x1,
        Disable = 0x2,
        Enable  = 0x3,
    };
};

struct GPIOControlInOverride {
    enum : u32 {
        Normal = 0x0,
        Invert = 0x1,
        Low    = 0x2,
        High   = 0x3,
    };
};

struct GPIOControl {
    enum : u32 {
        FuncSelect    = 0b0000'0000'0000'0000'0000'0000'0001'1111,
        OutOverride   = 0b0000'0000'0000'0000'0000'0011'0000'0000,
        OutEnOverride = 0b0000'0000'0000'0000'0011'0000'0000'0000,
        InOverride    = 0b0000'0000'0000'0011'0000'0000'0000'0000,
        IRQOverride   = 0b0011'0000'0000'0000'0000'0000'0000'0000,
    };
};
} // namespace iocommon
