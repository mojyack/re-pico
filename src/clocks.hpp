#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace clocks {
// gpio clock
struct GPIOClockAuxSource {
    enum : u32 {
        PLLSys = 0x0,
        GPIn0  = 0x1,
        GPIn1  = 0x2,
        PLLUSB = 0x3,
        ROSCPh = 0x4,
        XOSC   = 0x5,
        Sys    = 0x6,
        USB    = 0x7,
        ADC    = 0x8,
        RTC    = 0x9,
        Ref    = 0xa,
    };
};

struct GPIOClockControl {
    enum : u32 {
        AuxSource = 0b0000'0000'0000'0000'0000'0001'1110'0000,
        Kill      = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        Enable    = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        DC50      = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        Phase     = 0b0000'0000'0000'0011'0000'0000'0000'0000,
        Nudge     = 0b0000'0000'0001'0000'0000'0000'0000'0000,
    };
};

struct GPIOClockDiv {
    enum : u32 {
        Fractional = 0b0000'0000'0000'0000'0000'0000'1111'1111,
        Integer    = 0b1111'1111'1111'1111'1111'1111'0000'0000,
    };
};

// ref clock
struct RefClockSource {
    enum : u32 {
        ROSCPh = 0x0,
        Aux    = 0x1,
        XOSC   = 0x2,
    };
};

struct RefClockAuxSource {
    enum : u32 {
        PLLUSB = 0x0,
        GPIn0  = 0x1,
        GPIn1  = 0x2,
    };
};

struct RefClockControl {
    enum : u32 {
        Source    = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        AuxSource = 0b0000'0000'0000'0000'0000'0000'0110'0000,
    };
};

struct RefClockDiv {
    enum : u32 {
        Integer = 0b0000'0000'0000'0000'0000'0011'0000'0000,
    };
};

// sys clock
struct SysClockSource {
    enum : u32 {
        Ref = 0x0,
        Aux = 0x1,
    };
};

struct SysClockAuxSource {
    enum : u32 {
        PLLSys = 0x0,
        PLLUSB = 0x1,
        ROSCPh = 0x2,
        XOSC   = 0x3,
        GPIn0  = 0x4,
        GPIn1  = 0x5,
    };
};

struct SysClockControl {
    enum : u32 {
        Source    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        AuxSource = 0b0000'0000'0000'0000'0000'0000'1110'0000,
    };
};

struct SysClockDiv {
    enum : u32 {
        Fractional = 0b0000'0000'0000'0000'0000'0000'1111'1111,
        Integer    = 0b1111'1111'1111'1111'1111'1111'0000'0000,
    };
};

// peripheral clock
struct PeriClockAuxSource {
    enum : u32 {
        Sys    = 0x0,
        PLLSys = 0x1,
        PLLUSB = 0x2,
        ROSCPh = 0x3,
        XOSC   = 0x4,
        GPIn0  = 0x5,
        GPIn1  = 0x6,
    };
};

struct PeriClockControl {
    enum : u32 {
        AuxSource = 0b0000'0000'0000'0000'0000'0000'1110'0000,
        Kill      = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        Enable    = 0b0000'0000'0000'0000'0000'1000'0000'0000,
    };
};

// usb clock
struct USBClockAuxSource {
    enum : u32 {
        PLLUSB = 0x0,
        PLLSys = 0x1,
        ROSCPh = 0x2,
        XOSC   = 0x3,
        GPIn0  = 0x4,
        GPIn1  = 0x5,
    };
};

struct USBClockControl {
    enum : u32 {
        AuxSource = 0b0000'0000'0000'0000'0000'0000'1110'0000,
        Kill      = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        Enable    = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        Phase     = 0b0000'0000'0000'0011'0000'0000'0000'0000,
        Nudge     = 0b0000'0000'0001'0000'0000'0000'0000'0000,
    };
};

struct USBClockDiv {
    enum : u32 {
        Integer = 0b0000'0000'0000'0000'0000'0011'0000'0000,
    };
};

// adc clock
using ADCClockAuxSource = USBClockAuxSource;
using ADCClockControl   = USBClockControl;
using ADCClockDiv       = USBClockDiv;

// rtc clock
using RTCClockAuxSource = USBClockAuxSource;
using RTCClockControl   = USBClockControl;
using RTCClockDiv       = SysClockDiv;

// resus
struct ResusControl {
    enum : u32 {
        Timeout = 0b0000'0000'0000'0000'0000'0000'1111'1111,
        Enable  = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        Force   = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        Clear   = 0b0000'0000'0000'0001'0000'0000'0000'0000,
    };
};

struct ResusStatus {
    enum : u32 {
        Resussed = 0b0000'0000'0000'0000'0000'0000'0000'0001,
    };
};

// frequency counter
struct FC0Source {
    enum : u32 {
        Null   = 0x00,
        PLLSys = 0x01,
        PLLUSB = 0x02,
        ROSC   = 0x03,
        ROSCPh = 0x04,
        XOSC   = 0x05,
        GPIn0  = 0x06,
        GPIn1  = 0x07,
        Ref    = 0x08,
        Sys    = 0x09,
        Peri   = 0x0a,
        USB    = 0x0b,
        ADC    = 0x0c,
        RTC    = 0x0d,
    };
};

struct FC0Status {
    enum : u32 {
        Pass    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Done    = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        Running = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        Waiting = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        Fail    = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        Slow    = 0b0000'0000'0001'0000'0000'0000'0000'0000,
        Fast    = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        Died    = 0b0001'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct FC0Result {
    enum : u32 {
        Fractional = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        KHz        = 0b0011'1111'1111'1111'1111'1111'1111'0000,
    };
};

struct ClockRegs {
    v32  control;  // CLK_?_CTRL
    v32  div;      // CLK_?_DIV
    cv32 selected; // CLK_?_selected
};

// wake/sleep
struct WakeSleepClocks1 {
    enum : u32 {
        SysClocks           = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        ADCADC              = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SysADC              = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SysBusControl       = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        SysBusFabric        = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        SysDMA              = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        SysI2C0             = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        SysI2C1             = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        SysIO               = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        SysJTAG             = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        SysVregAndChipReset = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        SysPads             = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        SysPIO0             = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        SysPIO1             = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        SysPLLSys           = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        SysPLLUSB           = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        SysPSM              = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        SysPWM              = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        SysResets           = 0b0000'0000'0000'0100'0000'0000'0000'0000,
        SysROM              = 0b0000'0000'0000'1000'0000'0000'0000'0000,
        SysROSC             = 0b0000'0000'0001'0000'0000'0000'0000'0000,
        RTCRTC              = 0b0000'0000'0010'0000'0000'0000'0000'0000,
        SysRTC              = 0b0000'0000'0100'0000'0000'0000'0000'0000,
        SysSIO              = 0b0000'0000'1000'0000'0000'0000'0000'0000,
        PeriSPI0            = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        SysSPI0             = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PeriSPI1            = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        SysSPI1             = 0b0000'1000'0000'0000'0000'0000'0000'0000,
        SysSRAM0            = 0b0001'0000'0000'0000'0000'0000'0000'0000,
        SysSRAM1            = 0b0010'0000'0000'0000'0000'0000'0000'0000,
        SysSRAM2            = 0b0100'0000'0000'0000'0000'0000'0000'0000,
        SysSRAM3            = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct WakeSleepClocks2 {
    enum : u32 {
        SysSRAM4      = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        SysSRAM5      = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SysSysConfig  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SysSysInfo    = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        SysTBMan      = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        SysTimer      = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        PeriUART0     = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        SysUART0      = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        PeriUART1     = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        SysUART1      = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        SysUSBControl = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        USBUSBControl = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        SysWatchDog   = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        SysXIP        = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        SysXOSC       = 0b0000'0000'0000'0000'0100'0000'0000'0000,
    };
};

// interrupts
struct Interrupts {
    enum : u32 {
        SysResus = 0b0000'0000'0000'0000'0000'0000'0000'0001,
    };
};

struct Regs {
    ClockRegs clock_gpout0;
    ClockRegs clock_gpout1;
    ClockRegs clock_gpout2;
    ClockRegs clock_gpout3;
    ClockRegs clock_ref;
    ClockRegs clock_sys;
    ClockRegs clock_peri;
    ClockRegs clock_usb;
    ClockRegs clock_adc;
    ClockRegs clock_rtc;
    v32       clock_sys_resus_control;
    v32       clock_sys_resus_status;
    v32       fc0_ref_khz;
    v32       fc0_min_khz;
    v32       fc0_max_khz;
    v32       fc0_delay;
    v32       fc0_internal;
    v32       fc0_source;
    cv32      fc0_status;
    cv32      fc0_result;
    v32       wake_en0;
    v32       wake_en1;
    v32       sleep_en0;
    v32       sleep_en1;
    cv32      enabled0;
    cv32      enabled1;
    cv32      raw_interrupts;   // INTR
    v32       interrupt_enable; // INTE
    v32       interrupt_force;  // INTF
    cv32      interrupt_status; // INTS
};
} // namespace clocks

#define CLOCKS_REGS       (*(clocks::Regs*)(CLOCKS_BASE + 0x0000))
#define CLOCKS_REGS_XOR   (*(clocks::Regs*)(CLOCKS_BASE + 0x1000))
#define CLOCKS_REGS_SET   (*(clocks::Regs*)(CLOCKS_BASE + 0x2000))
#define CLOCKS_REGS_CLEAR (*(clocks::Regs*)(CLOCKS_BASE + 0x3000))
