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
#include <split.hpp>
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
    print_blocking(raw.data());
    return true;
}

template <noxx::comptime::String str, class... Args>
auto printf(const Args&... args) -> coop::Async<bool> {
    constexpr auto error_value = false;
    co_unwrap(raw, noxx::format<str>(noxx::move(args)...));
    co_await print(raw.data());
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
    printf_blocking<"- task={} parent={} suspend={} obj={} zombie={}\n">((void*)&task, (void*)task.parent, task.suspend_reason.get_index(), task.objective_of, task.zombie);
    for(auto i = usize(0); i < task.children.size(); i += 1) {
        dump_task_tree(*task.children[i], indent + 2);
    }
}

auto read_line() -> coop::Async<noxx::Optional<noxx::String>> {
    constexpr auto error_value = noxx::nullopt;

    auto line = noxx::String();
    while(true) {
        co_ensure(co_await coop::wait_for_io(uart::read_event));
        auto c = u8();
        co_ensure(uart::read({&c, 1}) == 1);
        co_await uart::write_all({&c, 1});
        if(c == '\r' || c == '\n') {
            break;
        } else {
            line.append(c);
        }
    }
    co_return line;
}

constexpr auto help = R"(commands:
  help              print this message
  reboot            reboot the board
  halow init|fw     control mm8108
    halow init      initialize pins
    halow fw        download firmware blob and bcf
  version           print dbgmcu versions
  ps                print process tree
  mem               dump heap chunks and usage
)";

auto handle_command(noxx::StringView line) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_unwrap(elms, split(line, " "));
    if(elms.size() == 0) {
        co_return true;
    }

    if(elms[0] == "help") {
        co_await print(help);
    } else if(elms[0] == "reboot") {
        SCB_REGS.app_int_control = BF(hw::scb::AppIntControl::VectKey, hw::scb::AppIntControlVectKey::Key) |
                                   BF(hw::scb::AppIntControl::SysResetReq, 1);
    } else if(elms[0] == "halow") {
        co_ensure(elms.size() >= 2);
        if(elms[1] == "init") {
            co_ensure(prepare_pins_for_halow());
            co_ensure(halow::init());
            co_await print("halow initialized\n");
        } else if(elms[1] == "fw") {
            co_unwrap(id, halow::read_u32(halow::Reg::ChipID));
            co_ensure(co_await printf<"halow chip id 0x{08x}\n">(id));
            co_unwrap(fw, halow::load_firmware());
            co_ensure(co_await printf<"halow host table 0x{08x} magic 0x{08x} fw {}.{}.{}\n">(
                fw.host_table_ptr, fw.magic,
                halow::version_major(fw.version),
                halow::version_minor(fw.version),
                halow::version_patch(fw.version)));
            if(fw.magic != halow::host_magic) {
                co_await print("halow magic mismatch, firmware did not boot\n");
            } else {
                co_await print("halow firmware booted\n");
            }
        } else {
            co_ensure(false, "invalid halow command");
        }
    } else if(elms[0] == "version") {
        const auto id  = FB(hw::dbgmcu::IDCode::DeviceID, DBGMCU_REGS.idcode);
        const auto rev = FB(hw::dbgmcu::IDCode::Revision, DBGMCU_REGS.idcode);
        co_ensure(co_await printf<"idcode 0x{04x} revision 0x{04x}\n">(id, rev));
    } else if(elms[0] == "ps") {
        dump_task_tree((co_await coop::reveal_runner())->root);
    } else if(elms[0] == "mem") {
        noxx::heap_walk(nullptr, [](void*, const void* addr, const usize size, const bool is_free) {
            printf_blocking<"  {} {} bytes {}\n">(addr, size, is_free ? "free" : "used");
        });
        const auto stats = noxx::heap_stats();
        co_ensure(co_await printf<"used {} bytes ({} chunks), free {} bytes ({} chunks), largest free {} bytes\n">(
            stats.used, stats.used_chunks, stats.free, stats.free_chunks, stats.largest_free));
    } else {
        co_ensure(co_await printf<"invalid command '{}': try help\n">(elms[0]));
    }

    co_return true;
}

auto console_task_main() -> coop::Async<bool> {
    constexpr auto error_value = false;
loop:
    co_await print("% ");
    co_unwrap(line, co_await read_line());
    co_await print("\n");
    if(line.size() == 0) {
        goto loop;
    }
    co_await handle_command(line);
    goto loop;
    co_return true;
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

    print_blocking("ready\n");
loop:
    auto runner = coop::Runner();
    runner.push_task(console_task_main());
    runner.run();
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
