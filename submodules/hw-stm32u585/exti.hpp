#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::exti {
// EXTICR, 8 bits per line, selects the gpio port driving the line
struct Port {
    enum : u32 {
        A = 0,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
    };
};

struct Regs {
    v32 rising_trigger;    // 0x00 RTSR1, 1 bit per line
    v32 falling_trigger;   // 0x04 FTSR1, 1 bit per line
    v32 sw_int_event;      // 0x08 SWIER1
    v32 rising_pending;    // 0x0C RPR1, write 1 to clear
    v32 falling_pending;   // 0x10 FPR1, write 1 to clear
    v32 secure_config;     // 0x14 SECCFGR1
    v32 priv_config;       // 0x18 PRIVCFGR1
    v32 reserved1[17];     // 0x1C
    v32 ext_int_select[4]; // 0x60 EXTICR1-4, 8 bits per line, Port; [n]=lines 4n..4n+3
    v32 lock;              // 0x70 LOCKR
    v32 reserved2[3];      // 0x74
    v32 int_mask;          // 0x80 IMR1, 1 bit per line
    v32 event_mask;        // 0x84 EMR1
};
static_assert(sizeof(Regs) == 0x84 + 4);

// field placement helpers for ext_int_select[1-4] (8 bits per line, 4 lines per register)
constexpr auto port(const u32 line, const u32 port) -> u32 {
    return port << (line % 4 * 8);
}
constexpr auto port_mask(const u32 line) -> u32 {
    return 0xff << (line % 4 * 8);
}
} // namespace hw::exti

#define EXTI_REGS (*(hw::exti::Regs*)(EXTI_BASE))
