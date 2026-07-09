#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::rcc {
struct Control {
    enum : u32 {
        MSISOn    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        MSIKEROn  = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        MSISReady = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        HSIOn     = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        HSIReady  = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        HSEOn     = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        HSEReady  = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        PLL1On    = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        PLL1Ready = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PLL2On    = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        PLL2Ready = 0b0000'1000'0000'0000'0000'0000'0000'0000,
        PLL3On    = 0b0001'0000'0000'0000'0000'0000'0000'0000,
        PLL3Ready = 0b0010'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Config1 {
    enum : u32 {
        SysClockSource = 0b0000'0000'0000'0000'0000'0000'0000'0011, // Config1SysClockSource
        SysClockStatus = 0b0000'0000'0000'0000'0000'0000'0000'1100, // Config1SysClockSource
    };
};

struct Config1SysClockSource {
    enum : u32 {
        MSIS  = 0,
        HSI16 = 1,
        HSE   = 2,
        PLL1R = 3,
    };
};

struct PLL1Config {
    enum : u32 {
        Source     = 0b0000'0000'0000'0000'0000'0000'0000'0011, // PLL1ConfigSource
        InputRange = 0b0000'0000'0000'0000'0000'0000'0000'1100, // PLL1ConfigInputRange
        FracEnable = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        M          = 0b0000'0000'0000'0000'0000'1111'0000'0000, // M - 1
        MBoost     = 0b0000'0000'0000'0000'1111'0000'0000'0000, // EPOD booster input divider, 0 = /1
        PEnable    = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        QEnable    = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        REnable    = 0b0000'0000'0000'0100'0000'0000'0000'0000,
    };
};

struct PLL1ConfigSource {
    enum : u32 {
        None  = 0,
        MSIS  = 1,
        HSI16 = 2,
        HSE   = 3,
    };
};

struct PLL1ConfigInputRange {
    enum : u32 {
        _4to8MHz  = 0,
        _8to16MHz = 3,
    };
};

struct PLL1Dividers {
    enum : u32 {
        N = 0b0000'0000'0000'0000'0000'0001'1111'1111, // N - 1
        P = 0b0000'0000'0000'0000'1111'1110'0000'0000, // P - 1
        Q = 0b0000'0000'0111'1111'0000'0000'0000'0000, // Q - 1
        R = 0b0111'1111'0000'0000'0000'0000'0000'0000, // R - 1
    };
};

struct AHB2Enable1 {
    enum : u32 {
        GPIOA = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        GPIOB = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        GPIOC = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        GPIOD = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        GPIOE = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        GPIOF = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        GPIOG = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        GPIOH = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        GPIOI = 0b0000'0000'0000'0000'0000'0001'0000'0000,
    };
};

struct AHB3Enable {
    enum : u32 {
        PWR = 0b0000'0000'0000'0000'0000'0000'0000'0100,
    };
};

struct APB1Enable1 {
    enum : u32 {
        SPI2 = 0b0000'0000'0000'0000'0100'0000'0000'0000,
    };
};

struct APB3Enable {
    enum : u32 {
        LPUART1 = 0b0000'0000'0000'0000'0000'0000'0100'0000,
    };
};

struct ClockConfig1 {
    enum : u32 {
        SPI2Source = 0b0000'0000'0000'0011'0000'0000'0000'0000, // ClockConfig1SPI2Source
    };
};

struct ClockConfig1SPI2Source {
    enum : u32 {
        PCLK1  = 0,
        SysClk = 1,
        HSI16  = 2,
        MSIK   = 3,
    };
};

struct ClockConfig3 {
    enum : u32 {
        LPUART1Source = 0b0000'0000'0000'0000'0000'0000'0000'0111, // ClockConfig3LPUART1Source
    };
};

struct ClockConfig3LPUART1Source {
    enum : u32 {
        PCLK3  = 0,
        SysClk = 1,
        HSI16  = 2,
        LSE    = 3,
        MSIK   = 4,
    };
};

struct Regs {
    v32  control;            // 0x00
    cv32 reserved0;          // 0x04
    v32  int_clock_control1; // 0x08
    v32  int_clock_control2; // 0x0C
    v32  int_clock_control3; // 0x10
    v32  recovery_control;   // 0x14
    cv32 reserved1;          // 0x18
    v32  config1;            // 0x1C
    v32  config2;            // 0x20
    v32  config3;            // 0x24
    v32  pll1_config;        // 0x28
    v32  pll2_config;        // 0x2C
    v32  pll3_config;        // 0x30
    v32  pll1_dividers;      // 0x34
    v32  pll1_frac;          // 0x38
    v32  pll2_dividers;      // 0x3C
    v32  pll2_frac;          // 0x40
    v32  pll3_dividers;      // 0x44
    v32  pll3_frac;          // 0x48
    cv32 reserved2;          // 0x4C
    v32  int_enable;         // 0x50
    cv32 int_flag;           // 0x54
    v32  int_clear;          // 0x58
    cv32 reserved3;          // 0x5C
    v32  ahb1_reset;         // 0x60
    v32  ahb2_reset1;        // 0x64
    v32  ahb2_reset2;        // 0x68
    v32  ahb3_reset;         // 0x6C
    cv32 reserved4;          // 0x70
    v32  apb1_reset1;        // 0x74
    v32  apb1_reset2;        // 0x78
    v32  apb2_reset;         // 0x7C
    v32  apb3_reset;         // 0x80
    cv32 reserved5;          // 0x84
    v32  ahb1_enable;        // 0x88
    v32  ahb2_enable1;       // 0x8C
    v32  ahb2_enable2;       // 0x90
    v32  ahb3_enable;        // 0x94
    cv32 reserved6;          // 0x98
    v32  apb1_enable1;       // 0x9C
    v32  apb1_enable2;       // 0xA0
    v32  apb2_enable;        // 0xA4
    v32  apb3_enable;        // 0xA8
    cv32 reserved7;          // 0xAC
    v32  ahb1_sleep_enable;  // 0xB0
    v32  ahb2_sleep_enable1; // 0xB4
    v32  ahb2_sleep_enable2; // 0xB8
    v32  ahb3_sleep_enable;  // 0xBC
    cv32 reserved8;          // 0xC0
    v32  apb1_sleep_enable1; // 0xC4
    v32  apb1_sleep_enable2; // 0xC8
    v32  apb2_sleep_enable;  // 0xCC
    v32  apb3_sleep_enable;  // 0xD0
    cv32 reserved9;          // 0xD4
    v32  srd_autonomous;     // 0xD8
    cv32 reserved10;         // 0xDC
    v32  clock_config1;      // 0xE0
    v32  clock_config2;      // 0xE4
    v32  clock_config3;      // 0xE8
    cv32 reserved11;         // 0xEC
    v32  backup_control;     // 0xF0
    v32  control_and_status; // 0xF4
    cv32 reserved12[6];      // 0xF8
    v32  secure_config;      // 0x110
    v32  privilege_config;   // 0x114
};
static_assert(sizeof(Regs) == 0x114 + 4);
} // namespace hw::rcc

#define RCC_REGS (*(hw::rcc::Regs*)(RCC_BASE))
