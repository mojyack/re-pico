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

#define _REGS (*(::Regs*)(_BASE))
