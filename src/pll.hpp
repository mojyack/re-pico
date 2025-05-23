#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace pll {
struct ControlAndStatus {
    enum : u32 {
        RefDiv = 0b0000'0000'0000'0000'0000'0000'0011'1111,
        Bypass = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        Lock   = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct PowerDown {
    enum : u32 {
        Core    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        DSM     = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        Postdiv = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        VCO     = 0b0000'0000'0000'0000'0000'0000'0010'0000,
    };
};

struct Primary {
    enum : u32 {
        Postdiv2 = 0b0000'0000'0000'0000'0111'0000'0000'0000,
        Postdiv1 = 0b0000'0000'0000'0111'0000'0000'0000'0000,
    };
};

struct Regs {
    v32 control_and_status; // CS
    v32 power_down;         // PWR
    v32 feedback_div;       // FBDIV_INT
    v32 primary;            // PRIM
};
} // namespace pll

#define PLL_SYS_REGS       (*(pll::Regs*)(PLL_SYS_BASE + 0x0000))
#define PLL_SYS_REGS_XOR   (*(pll::Regs*)(PLL_SYS_BASE + 0x1000))
#define PLL_SYS_REGS_SET   (*(pll::Regs*)(PLL_SYS_BASE + 0x2000))
#define PLL_SYS_REGS_CLEAR (*(pll::Regs*)(PLL_SYS_BASE + 0x3000))
#define PLL_USB_REGS       (*(pll::Regs*)(PLL_USB_BASE + 0x0000))
#define PLL_USB_REGS_XOR   (*(pll::Regs*)(PLL_USB_BASE + 0x1000))
#define PLL_USB_REGS_SET   (*(pll::Regs*)(PLL_USB_BASE + 0x2000))
#define PLL_USB_REGS_CLEAR (*(pll::Regs*)(PLL_USB_BASE + 0x3000))
