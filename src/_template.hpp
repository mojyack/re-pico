#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace ns {
struct Reg {
    enum : u32 {
        MASK = 0b0000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Regs {
};
static_assert(sizeof(Regs) == 0x0 + 4);
} // namespace ns

#define _REGS       (*(::Regs*)(_BASE + 0x0000))
#define _REGS_XOR   (*(::Regs*)(_BASE + 0x1000))
#define _REGS_SET   (*(::Regs*)(_BASE + 0x2000))
#define _REGS_CLEAR (*(::Regs*)(_BASE + 0x3000))
