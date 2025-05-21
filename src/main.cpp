#include "io-bank0.hpp"
#include "resets.hpp"
#include "sio.hpp"

namespace {
auto enable_gpio_25() -> void {
    RESETS_REGS.reset &= ~(resets::ResetNum::IO_BANK0);
    while(!(RESETS_REGS.reset_done & resets::ResetNum::IO_BANK0)) {
    }

    IOBANK0_REGS.status_control[25].control = iobank0::GPIOControlFuncSel::SIO;

    SIO_REGS.gpio_oe_set = 1 << 25;
}

auto entry() -> void {
    enable_gpio_25();
    while(true) {
        for(auto i = 0; i < 300000; i += 1) {
            SIO_REGS.gpio_out_set = 1 << 25;
        }
        for(auto i = 0; i < 300000; i += 1) {
            SIO_REGS.gpio_out_clr = 1 << 25;
        }
    }
}
} // namespace

extern "C" {
[[noreturn]] auto default_int_handler() -> void {
    enable_gpio_25();
    while(true) {
        for(auto i = 0; i < 50000; i += 1) {
            SIO_REGS.gpio_out_set = 1 << 25;
        }
        for(auto i = 0; i < 50000; i += 1) {
            SIO_REGS.gpio_out_clr = 1 << 25;
        }
    }
}

// internal interruptions
__attribute__((weak, alias("default_int_handler"))) auto nmi_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto hard_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pend_sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto systick_handler() -> void;
// external interruptions
__attribute__((weak, alias("default_int_handler"))) auto timer_irq_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto timer_irq_1_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto timer_irq_2_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto timer_irq_3_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pwm_irq_wrap_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto usb_control_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto xip_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pio_0_irq_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pio_0_irq_1_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pio_1_irq_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pio_1_irq_1_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto dma_irq_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto dma_irq_1_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto io_irq_bank_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto io_irq_qspi_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sio_irq_proc_0_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sio_irq_proc_1_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto clocks_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto spi_0_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto spi_1_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto uart_0_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto uart_1_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto adc_irq_fifo_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto i2c_0_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto i2c_1_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto rtc_irq_handler() -> void;

extern u32 stack_top;

__attribute__((section(".vector"))) void* vector[48] = {
    // internal
    (void*)&stack_top,
    (void*)&entry,
    (void*)&nmi_handler,
    (void*)&hard_fault_handler,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    (void*)&sv_call_handler,
    nullptr,
    nullptr,
    (void*)&pend_sv_call_handler,
    (void*)&systick_handler,
    // external
    (void*)&timer_irq_0_handler,
    (void*)&timer_irq_1_handler,
    (void*)&timer_irq_2_handler,
    (void*)&timer_irq_3_handler,
    (void*)&pwm_irq_wrap_handler,
    (void*)&usb_control_irq_handler,
    (void*)&xip_irq_handler,
    (void*)&pio_0_irq_0_handler,
    (void*)&pio_0_irq_1_handler,
    (void*)&pio_1_irq_0_handler,
    (void*)&pio_1_irq_1_handler,
    (void*)&dma_irq_0_handler,
    (void*)&dma_irq_1_handler,
    (void*)&io_irq_bank_0_handler,
    (void*)&io_irq_qspi_handler,
    (void*)&sio_irq_proc_0_handler,
    (void*)&sio_irq_proc_1_handler,
    (void*)&clocks_irq_handler,
    (void*)&spi_0_irq_handler,
    (void*)&spi_1_irq_handler,
    (void*)&uart_0_irq_handler,
    (void*)&uart_1_irq_handler,
    (void*)&adc_irq_fifo_handler,
    (void*)&i2c_0_irq_handler,
    (void*)&i2c_1_irq_handler,
    (void*)&rtc_irq_handler,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
};
}
