#include "clocks.hpp"
#include "io-bank0.hpp"
#include "math.hpp"
#include "pll.hpp"
#include "resets.hpp"
#include "rosc.hpp"
#include "sio.hpp"
#include "timer.hpp"
#include "wd.hpp"
#include "xosc.hpp"

#define BF(field, value) value << math::log2<field>

namespace {
auto wait_for_bit(cv32& reg, const u32 mask) -> void {
    while(!(reg & mask)) {
    }
}

auto unreset(const u32 reset_num) -> void {
    RESETS_REGS.reset &= ~reset_num;
    wait_for_bit(RESETS_REGS.reset_done, reset_num);
}

auto enable_gpio_25() -> void {
    unreset(resets::ResetNum::IO_BANK0);

    IOBANK0_REGS.status_control[25].control = iobank0::GPIOControlFuncSel::SIO;

    SIO_REGS.gpio_oe_set = 1 << 25;
}

auto init_system() -> void {
    // enable xosc
    XOSC_REGS.control |= BF(xosc::ControlEnable::ENABLE, xosc::Control::ENABLE);
    wait_for_bit(XOSC_REGS.status, xosc::Status::STABLE);
    // enable system pll
    unreset(resets::ResetNum::PLL_SYS);
    PLL_SYS_REGS.feedback_div = 100; // VCO clokc = 12MHz * 100 = 1.2GHz
    PLL_SYS_REGS.power_down &= ~(BF(pll::PowerDown::PD, 1) | BF(pll::PowerDown::VCOPD, 1));
    wait_for_bit(PLL_SYS_REGS.control_and_status, pll::ControlAndStatus::LOCK);
    PLL_SYS_REGS.primary = BF(pll::Primary::POSTDIV1, 6) | BF(pll::Primary::POSTDIV2, 2); // 1.2GHz / 6 / 2 = 100MHz
    PLL_SYS_REGS.power_down &= ~(BF(pll::PowerDown::POSTDIVPD, 1));
    // setup clock generators
    CLOCKS_REGS.clock_ref.control |= BF(clocks::RefClockControl::SRC, clocks::RefClockSource::XOSC);
    wait_for_bit(CLOCKS_REGS.clock_ref.selected, 1 << clocks::RefClockSource::XOSC);
    CLOCKS_REGS.clock_sys.control |= BF(clocks::SysClockControl::SRC, clocks::SysClockSource::Aux);
    wait_for_bit(CLOCKS_REGS.clock_sys.selected, 1 << clocks::SysClockSource::Aux);
    // stop rosc
    ROSC_REGS.control = ROSC_REGS.control & ~rosc::Control::ENABLE | BF(rosc::Control::ENABLE, rosc::ControlEnable::DISABLE);
    // enable 64-bit timer
    WATCHDOG_REGS.tick |= BF(wd::Tick::CYCLES, 12); // 1us = 12cycles / 12MHz
    unreset(resets::ResetNum::TIMER);
}

auto read_time() -> u64 {
    return TIMER_REGS.time_low_read | u64(TIMER_REGS.time_high_read) << 32;
}

auto usleep(u64 us) -> void {
    auto start = read_time();
    while(read_time() - start < us) {
    }
}

auto entry() -> void {
    enable_gpio_25();
    SIO_REGS.gpio_out_set = 1 << 25;
    init_system();
    while(true) {
        SIO_REGS.gpio_out_xor = 1 << 25;
        usleep(50000);
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
