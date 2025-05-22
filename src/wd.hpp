#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace wd {
struct Control {
    enum : u32 {
        TIME       = 0b0000'0000'1111'1111'1111'1111'1111'1111,
        PAUSE_JTAG = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        PAUSE_DBG0 = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PAUSE_DBG1 = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        ENABLE     = 0b0100'0000'0000'0000'0000'0000'0000'0000,
        TRIGGER    = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Reason {
    enum : u32 {
        TIMER = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        FORCE = 0b0000'0000'0000'0000'0000'0000'0000'0010,
    };
};

struct Tick {
    enum : u32 {
        CYCLES  = 0b0000'0000'0000'0000'0000'0001'1111'1111,
        ENABLE  = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        RUNNING = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        COUNT   = 0b0000'0000'0000'1111'1111'1000'0000'0000,
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
