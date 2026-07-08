#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

// USART/UART/LPUART share this register layout
// note: LPUART baud_rate = 256 * clock / baud (20 bits), USART baud_rate = clock / baud
namespace usart {
struct Control1 {
    enum : u32 {
        EnableUART     = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        EnableInStop   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        EnableRX       = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        EnableTX       = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        IdleInt        = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        RXNotEmptyInt  = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        TXCompleteInt  = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        TXEmptyInt     = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        ParityErrorInt = 0b0000'0000'0000'0000'0000'0001'0000'0000,
        ParityOdd      = 0b0000'0000'0000'0000'0000'0010'0000'0000,
        EnableParity   = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        WordLength0    = 0b0000'0000'0000'0000'0001'0000'0000'0000, // {M1,M0}: 00=8bit 01=9bit 10=7bit
        EnableMute     = 0b0000'0000'0000'0000'0010'0000'0000'0000,
        WordLength1    = 0b0001'0000'0000'0000'0000'0000'0000'0000,
        EnableFIFO     = 0b0010'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct Status {
    enum : u32 {
        ParityError  = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        FramingError = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        NoiseError   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        OverrunError = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        Idle         = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        RXNotEmpty   = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        TXComplete   = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        TXEmpty      = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        Busy         = 0b0000'0000'0000'0001'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  control1;         // 0x00
    v32  control2;         // 0x04
    v32  control3;         // 0x08
    v32  baud_rate;        // 0x0C
    v32  guard_time;       // 0x10
    v32  receiver_timeout; // 0x14
    v32  request;          // 0x18
    cv32 status;           // 0x1C
    v32  int_clear;        // 0x20
    cv32 receive_data;     // 0x24
    v32  transmit_data;    // 0x28
    v32  prescaler;        // 0x2C
    v32  autonomous;       // 0x30
};
static_assert(sizeof(Regs) == 0x30 + 4);
} // namespace usart

#define LPUART1_REGS (*(usart::Regs*)(LPUART1_BASE))
