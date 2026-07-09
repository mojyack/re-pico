#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::gpio {
// MODER, 2 bits per pin (reset state is Analog except some port A/B pins)
struct Mode {
    enum : u32 {
        Input     = 0,
        Output    = 1,
        Alternate = 2,
        Analog    = 3,
    };
};

// OSPEEDR, 2 bits per pin
struct Speed {
    enum : u32 {
        Low      = 0,
        Medium   = 1,
        High     = 2,
        VeryHigh = 3,
    };
};

// PUPDR, 2 bits per pin
struct Pull {
    enum : u32 {
        None = 0,
        Up   = 1,
        Down = 2,
    };
};

struct Regs {
    v32  mode;            // 0x00 2 bits per pin, Mode
    v32  output_type;     // 0x04 1 bit per pin, 0=push-pull 1=open-drain
    v32  output_speed;    // 0x08 2 bits per pin, Speed
    v32  pull;            // 0x0C 2 bits per pin, Pull
    cv32 input_data;      // 0x10
    v32  output_data;     // 0x14
    v32  bit_set_reset;   // 0x18 [15:0] set, [31:16] reset
    v32  config_lock;     // 0x1C
    v32  alt_function[2]; // 0x20 4 bits per pin, [0]=pins 0-7, [1]=pins 8-15
    v32  bit_reset;       // 0x28
    v32  high_speed_lv;   // 0x2C
    v32  secure_config;   // 0x30
};
static_assert(sizeof(Regs) == 0x30 + 4);

// field placement helpers
constexpr auto mode(const u32 pin, const u32 mode) -> u32 {
    return mode << (pin * 2);
}
constexpr auto mode_mask(const u32 pin) -> u32 {
    return 0b11 << (pin * 2);
}
constexpr auto alt_function(const u32 pin, const u32 af) -> u32 {
    return af << (pin % 8 * 4);
}
constexpr auto alt_function_mask(const u32 pin) -> u32 {
    return 0b1111 << (pin % 8 * 4);
}
} // namespace hw::gpio

#define GPIOA_REGS (*(hw::gpio::Regs*)(GPIOA_BASE))
#define GPIOB_REGS (*(hw::gpio::Regs*)(GPIOB_BASE))
#define GPIOC_REGS (*(hw::gpio::Regs*)(GPIOC_BASE))
#define GPIOD_REGS (*(hw::gpio::Regs*)(GPIOD_BASE))
#define GPIOE_REGS (*(hw::gpio::Regs*)(GPIOE_BASE))
#define GPIOF_REGS (*(hw::gpio::Regs*)(GPIOF_BASE))
#define GPIOG_REGS (*(hw::gpio::Regs*)(GPIOG_BASE))
#define GPIOH_REGS (*(hw::gpio::Regs*)(GPIOH_BASE))
#define GPIOI_REGS (*(hw::gpio::Regs*)(GPIOI_BASE))
