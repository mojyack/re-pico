#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace uart {
struct Data {
    enum : u32 {
        ReceivedData = 0b0000'0000'0000'0000'0000'0000'1111'1111,
        FramingError = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        ParityError  = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        BreakError   = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        OverrunError = 0b0000'0000'0000'0000'0000'1000'0000'0000,
    };
};

struct ReceiveStatusAndClear {
    enum : u32 {
        FramingError = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        ParityError  = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        BreakError   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        OverrunError = 0b0000'0000'0000'0000'0000'0000'0000'1000,
    };
};

struct Flag {
    enum : u32 {
        ClearToSend       = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        DataSetReady      = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        DataCarrierDetect = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        Busy              = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        RXFIFOEmpty       = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        TXFIFOFull        = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        RXFIFOFull        = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        TXFIFOEmpty       = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        RingIndicator     = 0b0000'0000'0000'0000'0000'0001'0000'0000,
    };
};

struct LineControl {
    enum : u32 {
        Break             = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        EnableParityBit   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        EvenParitySelect  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        TwoStopBitsSelect = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        EnableFIFO        = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        WordLength        = 0b0000'0000'0000'0000'0000'0000'0110'0000, // 5+xbits
        StickParitySelect = 0b0000'0000'0000'0000'0000'0000'1000'0000,
    };
};

struct Control {
    enum : u32 {
        EnableUART        = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        EnableSIR         = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SIRLowPowerMode   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        EnableLoopback    = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        EnableTX          = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        EnableRX          = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        DataTransmitReady = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        RequestToSend     = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        Out1              = 0b0000'0000'0000'0000'0001'0000'0000'0000,
        Out2              = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        EnableRTS         = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        EnableCTS         = 0b0000'0000'0000'0000'1000'0000'0000'0000,
    };
};

struct FIFOLevel {
    enum : u32 {
        _1o8 = 0,
        _1o4 = 1,
        _1o2 = 2,
        _3o4 = 3,
        _7o8 = 4,
    };
};

struct InterruptFifoLevelSelect {
    enum : u32 {
        TX = 0b0000'0000'0000'0000'0000'0000'0000'0111, // FIFOLevel
        RX = 0b0000'0000'0000'0000'0000'0000'0011'1000, // FIFOLevel
    };
};

struct InterruptMask {
    enum : u32 {
        RingIndicator            = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        ClearToSend              = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        DataChannelCarrierDetect = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        DataSetReady             = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        RX                       = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        TX                       = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        ReceiveTimeout           = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        FramingError             = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        ParityError              = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        BreakError               = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        OverrunError             = 0b0000'0000'0000'0000'0000'0100'0000'0000,
    };
};

struct DMAControl {
    enum : u32 {
        EnableRXDMA = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        EnableTXDMA = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        DMAOnError  = 0b0000'0000'0000'0000'0000'0000'0000'0100,
    };
};

struct Regs {
    v32  data;
    v32  receive_status_and_clear;
    cv32 reserved1[4];
    cv32 flag;
    cv32 reserved2[1];
    v32  low_power_counter;
    v32  integer_baud_rate;
    v32  fractional_baud_rate;
    v32  line_control;
    v32  control;
    v32  interrupt_fifo_level_select;
    v32  interrupt_mask_set_clear; // InterruptMask
    cv32 raw_interrupt_status;     // InterruptMask
    cv32 masked_interrupt_status;  // InterruptMask
    v32  interrupt_clear;          // InterruptMask
    v32  dma_control;
    cv32 reserved3[997];
    cv32 peripheral_id[4];
    cv32 cell_id[4];
};
} // namespace uart

#define UART0_REGS       (*(uart::Regs*)(UART0_BASE + 0x0000))
#define UART0_REGS_XOR   (*(uart::Regs*)(UART0_BASE + 0x1000))
#define UART0_REGS_SET   (*(uart::Regs*)(UART0_BASE + 0x2000))
#define UART0_REGS_CLEAR (*(uart::Regs*)(UART0_BASE + 0x3000))
#define UART1_REGS       (*(uart::Regs*)(UART1_BASE + 0x0000))
#define UART1_REGS_XOR   (*(uart::Regs*)(UART1_BASE + 0x1000))
#define UART1_REGS_SET   (*(uart::Regs*)(UART1_BASE + 0x2000))
#define UART1_REGS_CLEAR (*(uart::Regs*)(UART1_BASE + 0x3000))
