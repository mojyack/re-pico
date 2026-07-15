#include <coop/io.hpp>
#include <hal/uart.hpp>
#include <intrinsics.hpp>
#include <noxx/bits.hpp>

#include "../hw/gpio.hpp"
#include "../hw/nvic.hpp"
#include "../hw/rcc.hpp"
#include "../hw/usart.hpp"
#include "time.hpp"
#include "uart.hpp"

namespace uart {
namespace {
constexpr auto lpuart1_irq = u32(hw::nvic::IRQ::LpUart1);
constexpr auto rx_buf_size = usize(256); // must be a power of two

// single-producer (ISR) / single-consumer (read()) ring buffer.
// empty when head == tail, full when (head + 1) % size == tail.
volatile u8   rx_buf[rx_buf_size];
volatile auto rx_head = usize(0); // advanced by the ISR
volatile auto rx_tail = usize(0); // advanced by the reader
} // namespace

auto init(const uint baud_rate) -> void {
    // LPUART1 console: PC0 = RX, PC1 = TX (stlink vcp), same pins main uses
    RCC_REGS.ahb2_enable1 |= hw::rcc::AHB2Enable1::GPIOC;
    RCC_REGS.apb3_enable |= hw::rcc::APB3Enable::LPUART1;
    (void)RCC_REGS.apb3_enable;
    GPIOC_REGS.alt_function[0] = (GPIOC_REGS.alt_function[0] &
                                  ~(hw::gpio::alt_function_mask(0) | hw::gpio::alt_function_mask(1))) |
                                 hw::gpio::alt_function(0, 8) |
                                 hw::gpio::alt_function(1, 8); // AF8 = LPUART1
    GPIOC_REGS.mode            = (GPIOC_REGS.mode & ~(hw::gpio::mode_mask(0) | hw::gpio::mode_mask(1))) |
                                 hw::gpio::mode(0, hw::gpio::Mode::Alternate) |
                                 hw::gpio::mode(1, hw::gpio::Mode::Alternate);
    // lpuart kernel clock = PCLK3 = 160MHz, baud = 256 * clock / brr
    LPUART1_REGS.baud_rate = (u64(256) * time::cpu_hz + baud_rate / 2) / baud_rate;
    rx_head                = 0;
    rx_tail                = 0;
    LPUART1_REGS.control1  = BF(hw::usart::Control1::EnableUART, 1) |
                             BF(hw::usart::Control1::EnableTX, 1) |
                             BF(hw::usart::Control1::EnableRX, 1) |
                             BF(hw::usart::Control1::RXNotEmptyInt, 1); // fill rx_buf from the ISR
    // route the LPUART1 global interrupt to lpuart1_handler
    NVIC_REGS.set_enable[lpuart1_irq / 32] = 1 << (lpuart1_irq % 32);
}

auto deinit() -> void {
    NVIC_REGS.clear_enable[lpuart1_irq / 32] = 1 << (lpuart1_irq % 32);
    LPUART1_REGS.control1 &= ~BF(hw::usart::Control1::RXNotEmptyInt, 1);
}

auto lpuart1_handler() -> void {
    if(LPUART1_REGS.status & hw::usart::Status::RXNotEmpty) {
        while(LPUART1_REGS.status & hw::usart::Status::RXNotEmpty) {
            const auto c    = u8(LPUART1_REGS.receive_data);
            const auto next = (rx_head + 1) % rx_buf_size;
            if(next != rx_tail) {
                rx_buf[rx_head] = c;
                rx_head         = next;
            } else {
                // buffer full, drop the byte
            }
        }
        read_event.notify();
    }
    if((LPUART1_REGS.status & hw::usart::Status::TXEmpty) &&
       (LPUART1_REGS.control1 & hw::usart::Control1::TXEmptyInt)) {
        LPUART1_REGS.control1 &= ~BF(hw::usart::Control1::TXEmptyInt, 1);
        write_event.notify();
    }
    if(LPUART1_REGS.status & hw::usart::Status::OverrunError) {
        LPUART1_REGS.int_clear = hw::usart::Status::OverrunError;
    }
}

auto read(const noxx::Span<u8> buf) -> usize {
    auto n = usize(0);
    while(rx_head != rx_tail && n < buf.size()) {
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
    while((LPUART1_REGS.status & hw::usart::Status::TXEmpty) && n < buf.size()) {
        LPUART1_REGS.transmit_data = buf[n];
        n += 1;
    }
    return n;
}

auto read_available() -> bool {
    return rx_head != rx_tail;
}

auto write_available() -> bool {
    __disable_irq();
    const auto empty = LPUART1_REGS.status & hw::usart::Status::TXEmpty;
    if(!empty) {
        LPUART1_REGS.control1 |= BF(hw::usart::Control1::TXEmptyInt, 1);
    }
    __enable_irq();
    return empty;
}
} // namespace uart
