#pragma once
#include <noxx/int.hpp>

namespace nvic {
enum class IRQ : u32 {
    Timer0   = 0,
    Timer1   = 1,
    Timer2   = 2,
    Timer3   = 3,
    PwmWrap  = 4,
    UsbCtrl  = 5,
    Xip      = 6,
    Pio0Irq0 = 7,
    Pio0Irq1 = 8,
    Pio1Irq0 = 9,
    Pio1Irq1 = 10,
    Dma0     = 11,
    Dma1     = 12,
    IoBank0  = 13,
    IoQspi   = 14,
    SioProc0 = 15,
    SioProc1 = 16,
    Clocks   = 17,
    Spi0     = 18,
    Spi1     = 19,
    Uart0    = 20,
    Uart1    = 21,
    AdcFifo  = 22,
    I2c0     = 23,
    I2c1     = 24,
    Rtc      = 25,
};
} // namespace nvic
