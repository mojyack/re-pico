#include <coop/io.hpp>
#include <hal/uart.hpp>
#include <intrinsics.hpp>
#include <noxx/bits.hpp>

#include "../hw/clocks.hpp"
#include "../hw/io-bank0.hpp"
#include "../hw/m0plus.hpp"
#include "../hw/nvic.hpp"
#include "../hw/resets.hpp"
#include "../hw/uart.hpp"
#include "../system.hpp"
#include "time.hpp"
#include "uart.hpp"

namespace uart {
namespace {
constexpr auto uart0_irq   = u32(nvic::IRQ::Uart0);
constexpr auto rx_buf_size = usize(256); // must be a power of two

// single-producer (ISR) / single-consumer (read()) ring buffer.
// empty when head == tail, full when (head + 1) % size == tail.
volatile u8   rx_buf[rx_buf_size];
volatile auto rx_head = usize(0); // advanced by the ISR
volatile auto rx_tail = usize(0); // advanced by the reader

struct Div {
    u32 i;
    u32 f;
};

auto calc_baud_rate_divisor(const u32 clk_peri, const u32 baud) -> Div {
    // const auto div = 1. * clk_peri / (baud * (1 << 4));
    // return {u32(div), u32(div * 64 + 0.5)};
    const auto div = (1 << 3) * clk_peri / baud + 1;
    return {div >> 7, (div & 0b1111111) >> 1};
}
} // namespace

auto init(const uint baud_rate) -> void {
    // GPIO0 = TX, GPIO1 = RX
    IO_BANK0_REGS.status_control[0].control = BF(iobank0::GPIOControl::FuncSelect, iobank0::GPIOControlFuncSelect::UART);
    IO_BANK0_REGS.status_control[1].control = BF(iobank0::GPIOControl::FuncSelect, iobank0::GPIOControlFuncSelect::UART);
    CLOCKS_REGS_SET.clock_peri.control      = BF(clocks::PeriClockControl::Enable, 1);
    unreset(resets::ResetNum::UART0);
    const auto div                  = calc_baud_rate_divisor(time::clk_peri_hz, baud_rate);
    UART0_REGS.integer_baud_rate    = div.i;
    UART0_REGS.fractional_baud_rate = div.f;
    UART0_REGS.line_control =
        BF(uart::LineControl::WordLength, 8 - 5) |
        BF(uart::LineControl::EnableFIFO, 1);
    rx_head                             = 0;
    rx_tail                             = 0;
    UART0_REGS.interrupt_mask_set_clear = uart::InterruptMask::RX | uart::InterruptMask::ReceiveTimeout; // fill rx_buf from the ISR
    UART0_REGS_SET.control              = BF(uart::Control::EnableUART, 1);                              // TX and RX are default enabled
    // route the UART0 interrupt to uart0_handler
    M0PLUS_REGS.nvic_set_enable = 1 << uart0_irq;
}

auto deinit() -> void {
    M0PLUS_REGS.nvic_clear_enable       = 1 << uart0_irq;
    UART0_REGS.interrupt_mask_set_clear = 0;
}

auto uart0_handler() -> void {
    const auto status = UART0_REGS.masked_interrupt_status;
    if(status & (uart::InterruptMask::RX | uart::InterruptMask::ReceiveTimeout)) {
        while(!(UART0_REGS.flag & uart::Flag::RXFIFOEmpty)) {
            const auto c    = u8(UART0_REGS.data);
            const auto next = (rx_head + 1) % rx_buf_size;
            if(next != rx_tail) {
                rx_buf[rx_head] = c;
                rx_head         = next;
            } else {
                // buffer full, drop the byte
            }
        }
        UART0_REGS.interrupt_clear = uart::InterruptMask::ReceiveTimeout; // rx cleared by draining the fifo
        read_event.notify();
    }
    if(status & uart::InterruptMask::TX) {
        UART0_REGS_CLEAR.interrupt_mask_set_clear = uart::InterruptMask::TX;
        write_event.notify();
    }
    if(status & uart::InterruptMask::OverrunError) {
        UART0_REGS.interrupt_clear = uart::InterruptMask::OverrunError;
    }
}

auto read(const noxx::Span<u8> buf) -> usize {
    auto n = usize(0);
    while(rx_head != rx_tail && n < buf.size) {
        __disable_irq();
        buf[n]  = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % rx_buf_size;
        __enable_irq();
        n += 1;
    }
    return n;
}

auto write(const noxx::Span<const u8> buf) -> usize {
    auto n = usize(0);
    while(!(UART0_REGS.flag & uart::Flag::TXFIFOFull) && n < buf.size) {
        UART0_REGS.data = buf[n];
        n += 1;
    }
    return n;
}

auto read_available() -> bool {
    return rx_head != rx_tail;
}

auto write_available() -> bool {
    __disable_irq();
    const auto ready = !(UART0_REGS.flag & uart::Flag::TXFIFOFull);
    if(!ready) {
        UART0_REGS_SET.interrupt_mask_set_clear = uart::InterruptMask::TX;
    }
    __enable_irq();
    return ready;
}
} // namespace uart
