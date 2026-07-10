#include <coop/io.hpp>
#include <coop/promise.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <hal/uart.hpp>
#include <halow/firmware.hpp>
#include <halow/halow.hpp>
#include <noxx/bits.hpp>
#include <noxx/format.hpp>
#include <noxx/malloc.hpp>
#include <print.hpp>
#include <uart.hpp>

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
auto printf_blocking(const Args&... args) -> bool {
    constexpr auto error_value = false;
    unwrap(raw, noxx::format<str>(noxx::move(args)...));
    println_blocking(raw.data());
    return true;
}

template <noxx::comptime::String str, class... Args>
auto printf(const Args&... args) -> coop::Async<bool> {
    constexpr auto error_value = false;
    co_unwrap(raw, noxx::format<str>(noxx::move(args)...));
    co_await println(raw.data());
    co_return true;
}

auto get_u8() -> u8 {
    auto buf = noxx::Array<u8, 1>();
    uart::read_blocking(buf);
    return buf[0];
}

auto dump_task_tree(const coop::Task& task, const int indent = 0) -> void {
    for(auto i = 0; i < indent; i += 1) {
        print_blocking(" ");
    }
    printf_blocking<"- task={} parent={} suspend={} obj={} zombie={}">((void*)&task, (void*)task.parent, task.suspend_reason.get_index(), task.objective_of, task.zombie);
    for(auto i = usize(0); i < task.children.size(); i += 1) {
        dump_task_tree(*task.children[i], indent + 2);
    }
}

auto coop_demo() -> bool {
    struct S {
        static auto blink(const u32 pin, const u32 interval_ms) -> coop::Async<void> {
        loop:
            GPIOE_REGS.output_data ^= 1 << pin;
            co_await coop::sleep_ms(interval_ms);
            goto loop;
        }

        static auto echo() -> coop::Async<bool> {
            constexpr auto error_value = false;

            auto buf = noxx::Array<u8, 4>();
        loop:
            co_ensure(co_await coop::wait_for_io(uart::read_event));
            auto n = uart::read(buf);
            while(n > 0) {
                co_ensure(co_await printf<"read '{}'">(noxx::StringView((char*)buf.data, n)));
                n = uart::read(buf);
            }
            goto loop;
        }

        static auto timer(const u32 time, noxx::Span<coop::TaskHandle> handles) -> coop::Async<void> {
            co_await coop::sleep_ms(time);
            for(auto i = 0uz; i < handles.size; i += 1) {
                handles[i].cancel();
            }
        }
    };

    constexpr auto error_value = false;
    auto           runner      = coop::Runner();
    auto           handles     = noxx::Array<coop::TaskHandle, 3>();
    ensure(runner.push_task(S::blink(led_green, 100), &handles[0]));
    ensure(runner.push_task(S::blink(led_red, 250), &handles[1]));
    ensure(runner.push_task(S::echo(), &handles[2]));
    ensure(runner.push_task(S::timer(5000, handles)));
    const auto begin = time::now();
    dump_task_tree(runner.root);
    ensure(runner.run());
    printf_blocking<"elapsed {}us">(time::now() - begin);
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

    println_blocking("ready");
    print_blocking("> ");
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
        printf_blocking<"{} {}">("hello", "world");
    } break;
    case 'h': {
        ensure(prepare_pins_for_halow());
        ensure(halow::init());
        println_blocking("halow initialized");
    } break;
    case 'f': {
        unwrap(id, halow::read_u32(halow::Reg::ChipID));
        printf_blocking<"halow chip id 0x{08x}">(id);
        unwrap(fw, halow::load_firmware());
        printf_blocking<"halow host table 0x{08x} magic 0x{08x} fw {}.{}.{}">(
            fw.host_table_ptr, fw.magic,
            halow::version_major(fw.version),
            halow::version_minor(fw.version),
            halow::version_patch(fw.version));
        if(fw.magic != halow::host_magic) {
            println_blocking("halow magic mismatch, firmware did not boot");
        } else {
            println_blocking("halow firmware booted");
        }
    } break;
    case 'c': {
        ensure(coop_demo());
        println_blocking("coop demo done");
    } break;
    case 'v': {
        const auto id  = FB(hw::dbgmcu::IDCode::DeviceID, DBGMCU_REGS.idcode);
        const auto rev = FB(hw::dbgmcu::IDCode::Revision, DBGMCU_REGS.idcode);
        printf_blocking<"idcode 0x{04x} revision 0x{04x}">(id, rev);
    } break;
#pragma pop_macro("error_act")
    }
    print_blocking("> ");
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
