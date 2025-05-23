#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace wd {
struct Control {
    enum : u32 {
        Time        = 0b0000'0000'1111'1111'1111'1111'1111'1111,
        PauseJTAG   = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        PauseDebug0 = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PauseDebug1 = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        Enable      = 0b0100'0000'0000'0000'0000'0000'0000'0000,
        Trigger     = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Reason {
    enum : u32 {
        Timer = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Force = 0b0000'0000'0000'0000'0000'0000'0000'0010,
    };
};

struct Tick {
    enum : u32 {
        Cycles  = 0b0000'0000'0000'0000'0000'0001'1111'1111,
        Enable  = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        Running = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        Count   = 0b0000'0000'0000'1111'1111'1000'0000'0000,
    };
};

struct Regs {
    v32 control;
    v32 load;
    v32 reason;
    v32 scratch[8];
    v32 tick;
};
} // namespace wd

#define WATCHDOG_REGS (*(wd::Regs*)(WATCHDOG_BASE))
