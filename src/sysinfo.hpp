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

#define SYSINFO_REGS (*(sysinfo::Regs*)(SYSINFO_BASE))
