#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace pll {
struct ControlAndStatus {
    enum : u32 {
        REFDIV = 0b0000'0000'0000'0000'0000'0000'0011'1111,
        BYPASS = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        LOCK   = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct PowerDown {
    enum : u32 {
        PD        = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        DSMPD     = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        POSTDIVPD = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        VCOPD     = 0b0000'0000'0000'0000'0000'0000'0010'0000,
    };
};

struct Primary {
    enum : u32 {
        POSTDIV2 = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        POSTDIV1 = 0b0000'0000'0000'0111'0000'0000'0000'0000,
    };
};

struct Regs {
    v32 control_and_status; // CS
    v32 power_down;         // PWR
    v32 feedback_div;       // FBDIV_INT
    v32 primary;            // PRIM
};
} // namespace pll

#define PLL_SYS_REGS (*(pll::Regs*)(PLL_SYS_BASE))
#define PLL_USB_REGS (*(pll::Regs*)(PLL_USB_BASE))
