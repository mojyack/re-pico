// MM8108-EKH05 (STM32U585AI host MCU) port
#include "hw/flash.hpp"
#include "hw/gpio.hpp"
#include "hw/m33.hpp"
#include "hw/pwr.hpp"
#include "hw/rcc.hpp"
#include "hw/usart.hpp"
#include "noxx/bits.hpp"
#include "noxx/format.hpp"
#include "noxx/malloc.hpp"

#include "noxx/assert.hpp"

// from linker script
extern u32 heap_start;
extern u32 stack_top;
extern u32 bss_start;
extern u32 bss_end;
extern u32 data_start;
extern u32 data_end;
extern u32 data_load;

namespace {
constexpr auto sys_clock = u32(160'000'000);

constexpr auto led_green = u32(7);  // PE7
constexpr auto led_blue  = u32(8);  // PE8
constexpr auto led_red   = u32(11); // PE11

auto wait_for_bit(cv32& reg, const u32 mask) -> void {
    while(!(reg & mask)) {
    }
}

auto enable_leds() -> void {
    RCC_REGS.ahb2_enable1 |= rcc::AHB2Enable1::GPIOE;
    GPIOE_REGS.mode = (GPIOE_REGS.mode &
                       ~(gpio::mode_mask(led_green) | gpio::mode_mask(led_blue) | gpio::mode_mask(led_red))) |
                      gpio::mode(led_green, gpio::Mode::Output) |
                      gpio::mode(led_blue, gpio::Mode::Output) |
                      gpio::mode(led_red, gpio::Mode::Output);
}

auto init_system() -> void {
    // raise core voltage to range 1 (required for 160MHz)
    RCC_REGS.ahb3_enable |= rcc::AHB3Enable::PWR;
    PWR_REGS.voltage_scaling = BF(pwr::VoltageScaling::VOS, pwr::VoltageScalingVOS::Range1);
    wait_for_bit(PWR_REGS.voltage_scaling, pwr::VoltageScaling::VOSReady);
    // 4 wait states for 160MHz at voltage range 1
    FLASH_REGS.access_control = BF(flash::AccessControl::Latency, 4);
    while(FB(flash::AccessControl::Latency, FLASH_REGS.access_control) != 4) {
    }
    // configure system pll input (MSIS is 4MHz at reset), which also clocks the EPOD booster
    RCC_REGS.pll1_config =
        BF(rcc::PLL1Config::Source, rcc::PLL1ConfigSource::MSIS) |
        BF(rcc::PLL1Config::InputRange, rcc::PLL1ConfigInputRange::_4to8MHz) |
        BF(rcc::PLL1Config::M, 1 - 1) |
        BF(rcc::PLL1Config::MBoost, 0 /*EPOD input = 4MHz*/);
    // enable the EPOD booster (required for >55MHz)
    PWR_REGS.voltage_scaling |= BF(pwr::VoltageScaling::BoostEnable, 1);
    wait_for_bit(PWR_REGS.voltage_scaling, pwr::VoltageScaling::BoostReady);
    RCC_REGS.pll1_dividers =
        BF(rcc::PLL1Dividers::N, 80 - 1) | // VCO = 4MHz * 80 = 320MHz
        BF(rcc::PLL1Dividers::P, 2 - 1) |
        BF(rcc::PLL1Dividers::Q, 2 - 1) |
        BF(rcc::PLL1Dividers::R, 2 - 1); // sysclk = 320MHz / 2 = 160MHz
    RCC_REGS.pll1_config |= BF(rcc::PLL1Config::REnable, 1);
    RCC_REGS.control |= rcc::Control::PLL1On;
    wait_for_bit(RCC_REGS.control, rcc::Control::PLL1Ready);
    // switch sysclk to pll (AHB/APB dividers stay at /1)
    RCC_REGS.config1 = BF(rcc::Config1::SysClockSource, rcc::Config1SysClockSource::PLL1R);
    while(FB(rcc::Config1::SysClockStatus, RCC_REGS.config1) != rcc::Config1SysClockSource::PLL1R) {
    }
}

auto usleep(u64 us) -> void {
    SYSTICK_REGS.reload             = sys_clock / 1'000'000 - 1; // 1us per wrap
    SYSTICK_REGS.current            = 0;
    SYSTICK_REGS.control_and_status = BF(systick::ControlAndStatus::Enable, 1) |
                                      BF(systick::ControlAndStatus::CPUClockSource, 1);
    while(us > 0) {
        if(SYSTICK_REGS.control_and_status & systick::ControlAndStatus::CountFlag) {
            us -= 1;
        }
    }
    SYSTICK_REGS.control_and_status = 0;
}

auto print(const char* str) -> void {
    while(*str != '\0') {
        wait_for_bit(LPUART1_REGS.status, usart::Status::TXEmpty);
        LPUART1_REGS.transmit_data = *str;
        str += 1;
    }
}

auto println(const char* const str) -> void {
    print(str);
    print("\r\n");
}

template <noxx::comptime::String str, class... Args>
auto println(const Args&... args) -> bool {
#define error_act return false
    unwrap(raw, noxx::format<str>(noxx::move(args)...));
    print(raw.data());
    print("\r\n");
    return true;
#undef error_act
}

auto entry() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
    SCB_REGS.vector_table_offset = FLASH_BASE;
    const auto heap_end          = (usize)&stack_top - 8 * 1024; // 8KB for stack
    noxx::set_heap(&heap_start, heap_end - (usize)&heap_start);
    enable_leds();
    GPIOE_REGS.bit_set_reset = 1 << led_blue;
    init_system();

    // setup lpuart1 console (PC0 = RX, PC1 = TX, connected to stlink vcp)
    RCC_REGS.ahb2_enable1 |= rcc::AHB2Enable1::GPIOC;
    RCC_REGS.apb3_enable |= rcc::APB3Enable::LPUART1;
    (void)RCC_REGS.apb3_enable;
    GPIOC_REGS.alt_function[0] = (GPIOC_REGS.alt_function[0] &
                                  ~(gpio::alt_function_mask(0) | gpio::alt_function_mask(1))) |
                                 gpio::alt_function(0, 8) |
                                 gpio::alt_function(1, 8); // AF8 = LPUART1
    GPIOC_REGS.mode            = (GPIOC_REGS.mode & ~(gpio::mode_mask(0) | gpio::mode_mask(1))) |
                                 gpio::mode(0, gpio::Mode::Alternate) |
                                 gpio::mode(1, gpio::Mode::Alternate);
    // lpuart kernel clock = PCLK3 = 160MHz, baud = 256 * clock / brr
    LPUART1_REGS.baud_rate = (u64(256) * sys_clock + 115200 / 2) / 115200;
    LPUART1_REGS.control1  = BF(usart::Control1::EnableUART, 1) |
                             BF(usart::Control1::EnableTX, 1) |
                             BF(usart::Control1::EnableRX, 1);

    println("ready");
    while(true) {
        if(!(LPUART1_REGS.status & usart::Status::RXNotEmpty)) {
            usleep(50000);
            continue;
        }
        switch(u8(LPUART1_REGS.receive_data)) {
        case 'x':
            SCB_REGS.app_int_control = BF(scb::AppIntControl::VectKey, scb::AppIntControlVectKey::Key) |
                                       BF(scb::AppIntControl::SysResetReq, 1);
            break;
        case 'l':
            GPIOE_REGS.output_data ^= 1 << led_green;
            break;
        case 's': {
            println<"{} {}">("hello", "world");
        } break;
        case 'v': {
            const auto id  = FB(dbgmcu::IDCode::DeviceID, DBGMCU_REGS.idcode);
            const auto rev = FB(dbgmcu::IDCode::Revision, DBGMCU_REGS.idcode);
            println<"idcode 0x{04x} revision 0x{04x}">(id, rev);
        } break;
        }
    }
}
} // namespace

extern "C" {
[[noreturn]] auto default_int_handler() -> void {
    enable_leds();
    while(true) {
        for(auto i = 0; i < 500000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << led_red;
        }
        for(auto i = 0; i < 500000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << (led_red + 16);
        }
    }
}

// internal interruptions
__attribute__((weak, alias("default_int_handler"))) auto nmi_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto hard_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto mem_manage_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto bus_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto usage_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto secure_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto debug_monitor_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pend_sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto systick_handler() -> void;

// external interrupts are unused for now, faults are enough
__attribute__((section(".vector"))) void* vector[16] = {
    (void*)&stack_top,
    (void*)&entry,
    (void*)&nmi_handler,
    (void*)&hard_fault_handler,
    (void*)&mem_manage_handler,
    (void*)&bus_fault_handler,
    (void*)&usage_fault_handler,
    (void*)&secure_fault_handler,
    nullptr,
    nullptr,
    nullptr,
    (void*)&sv_call_handler,
    (void*)&debug_monitor_handler,
    nullptr,
    (void*)&pend_sv_call_handler,
    (void*)&systick_handler,
};
}

// noxx support
namespace noxx {
auto console_out(const char* ptr) -> bool {
    print(ptr);
    return true;
}

auto memcpy(void* dest, const void* src, usize size) -> void {
    for(auto i = usize(0); i < size; i += 1) {
        ((u8*)dest)[i] = ((const u8*)src)[i];
    }
}
} // namespace noxx
