#include <coop/coop.hpp>
#include <halow/firmware.hpp>
#include <halow/halow.hpp>
#include <noxx/bits.hpp>
#include <noxx/format.hpp>
#include <noxx/malloc.hpp>
#include <print.hpp>
#include <uart.hpp>
#include <hal/uart.hpp>

#include "hal/time.hpp"
#include "hal/uart.hpp"
#include "halow.hpp"
#include "hw/dbgmcu.hpp"
#include "hw/gpio.hpp"
#include "hw/nvic.hpp"
#include "hw/scb.hpp"
#include "system.hpp"

#include <noxx/assert.hpp>

// from link.ld
extern "C" void* vector[];
extern u32       heap_start;
extern u32       stack_top;
extern u32       bss_start;
extern u32       bss_end;
extern u32       data_start;
extern u32       data_end;
extern u32       data_load;

namespace {
template <noxx::comptime::String str, class... Args>
auto printf(const Args&... args) -> bool {
    constexpr auto error_value = false;
    unwrap(raw, noxx::format<str>(noxx::move(args)...));
    println(raw.data());
    return true;
}

auto blink(const u32 pin, const u32 interval_ms, const u32 count) -> coop::Async<void> {
    for(auto i = u32(0); i < count; i += 1) {
        GPIOE_REGS.output_data ^= 1 << pin;
        co_await coop::sleep_ms(interval_ms);
    }
}

auto get_u8() -> u8 {
    auto buf = noxx::Array<u8, 1>();
    uart::read_all(buf);
    return buf[0];
}

auto coop_demo() -> bool {
    constexpr auto error_value = false;
    auto           runner      = coop::Runner();
    ensure(runner.push_task(blink(led_green, 100, 20)));
    ensure(runner.push_task(blink(led_red, 250, 8)));
    const auto begin = time::now();
    ensure(runner.run());
    printf<"elapsed {}us">(time::now() - begin);
    return true;
}

auto entry() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
    SCB_REGS.vector_table_offset = u32(usize(&vector[0]));       // flash or SRAM, per link script
    const auto heap_end          = (usize)&stack_top - 8 * 1024; // 8KB for stack
    noxx::set_heap(&heap_start, heap_end - (usize)&heap_start);
    enable_leds();
    GPIOE_REGS.bit_set_reset = 1 << led_blue;
    init_system();
    uart::init(921600);
    time::start_systick();

    println("ready");
    print("> ");
loop:
    switch(get_u8()) {
#pragma push_macro("error_act")
#define error_act break
    case 'x':
        SCB_REGS.app_int_control = BF(hw::scb::AppIntControl::VectKey, hw::scb::AppIntControlVectKey::Key) |
                                   BF(hw::scb::AppIntControl::SysResetReq, 1);
        break;
    case 'l':
        GPIOE_REGS.output_data ^= 1 << led_green;
        break;
    case 's': {
        printf<"{} {}">("hello", "world");
    } break;
    case 'h': {
        ensure(prepare_pins_for_halow());
        ensure(halow::init());
        println("halow initialized");
    } break;
    case 'f': {
        unwrap(id, halow::read_u32(halow::Reg::ChipID));
        printf<"halow chip id 0x{08x}">(id);
        unwrap(fw, halow::load_firmware());
        printf<"halow host table 0x{08x} magic 0x{08x} fw {}.{}.{}">(
            fw.host_table_ptr, fw.magic,
            halow::version_major(fw.version),
            halow::version_minor(fw.version),
            halow::version_patch(fw.version));
        if(fw.magic != halow::host_magic) {
            println("halow magic mismatch, firmware did not boot");
        } else {
            println("halow firmware booted");
        }
    } break;
    case 'c': {
        ensure(coop_demo());
        println("coop demo done");
    } break;
    case 'v': {
        const auto id  = FB(hw::dbgmcu::IDCode::DeviceID, DBGMCU_REGS.idcode);
        const auto rev = FB(hw::dbgmcu::IDCode::Revision, DBGMCU_REGS.idcode);
        printf<"idcode 0x{04x} revision 0x{04x}">(id, rev);
    } break;
#pragma pop_macro("error_act")
    }
    print("> ");
    goto loop;
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

__attribute__((section(".vector"))) void* vector[16 + u32(hw::nvic::IRQ::LpUart1) + 1] = {
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
    [16 + u32(hw::nvic::IRQ::LpUart1)] = (void*)&uart::lpuart1_handler,
};
}
