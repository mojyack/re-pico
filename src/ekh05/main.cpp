#include <halow/firmware.hpp>
#include <halow/halow.hpp>
#include <noxx/bits.hpp>
#include <noxx/format.hpp>
#include <noxx/malloc.hpp>

#include "hal/sleep.hpp"
#include "halow.hpp"
#include "hw/gpio.hpp"
#include "hw/m33.hpp"
#include "hw/usart.hpp"
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
    init_uart(921600);

    println("ready");
    print("> ");
loop:
    if(!(LPUART1_REGS.status & hw::usart::Status::RXNotEmpty)) {
        usleep(50000);
        goto loop;
    }
    switch(u8(LPUART1_REGS.receive_data)) {
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
