#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace sysinfo {
struct ChipID {
    enum : u32 {
        Manufacture = 0b0000'0000'0000'0000'0000'1111'1111'1111,
        Part        = 0b0000'1111'1111'1111'1111'0000'0000'0000,
        Revision    = 0b1111'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Platform {
    enum : u32 {
        FPGA = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        ASIC = 0b0000'0000'0000'0000'0000'0000'0000'0010,
    };
};

struct Regs {
    cv32 chip_id;
    cv32 platform;
    cv32 reserved1[14];
    cv32 gitref_rp2040;
};
} // namespace sysinfo

#define SYSINFO_REGS       (*(sysnifo::Regs*)(SYSINFO_BASE + 0x0000))
#define SYSINFO_REGS_XOR   (*(sysnifo::Regs*)(SYSINFO_BASE + 0x1000))
#define SYSINFO_REGS_SET   (*(sysnifo::Regs*)(SYSINFO_BASE + 0x2000))
#define SYSINFO_REGS_CLEAR (*(sysnifo::Regs*)(SYSINFO_BASE + 0x3000))
