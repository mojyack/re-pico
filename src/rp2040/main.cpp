#include <console.hpp>
#include <coop/ext-event.hpp>
#include <coop/promise.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <hal/uart.hpp>
#include <noxx/bits.hpp>
#include <noxx/format.hpp>
#include <noxx/malloc.hpp>
#include <noxx/string.hpp>
#include <print.hpp>
#include <split.hpp>
#include <uart.hpp>

#include "hal/uart.hpp"
#include "hw/m0plus.hpp"
#include "hw/rom.hpp"
#include "system.hpp"

#include <noxx/assert.hpp>

// from linker script
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

auto dump_task_tree(const coop::Task& task, const int indent = 0) -> void {
    for(auto i = 0; i < indent; i += 1) {
        print_blocking(" ");
    }
    printf_blocking<"- task={} parent={} suspend={} obj={} zombie={}\n">((void*)&task, (void*)task.parent, task.suspend_reason.get_index(), task.objective_of, task.zombie);
    for(auto i = usize(0); i < task.children.size(); i += 1) {
        dump_task_tree(*task.children[i], indent + 2);
    }
}

constexpr auto help = R"(commands:
  help              print this message
  reboot            reboot the board
  bootsel           reboot into usb bootloader
  version           print rom version and copyright
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
        M0PLUS_REGS.app_int_and_reset_control =
            BF(m0plus::AppIntAndResetControl::VECTKey, 0x05fa) |
            BF(m0plus::AppIntAndResetControl::SysResetReq, 1);
    } else if(elms[0] == "bootsel") {
        ((rom::reset_to_usb_boot*)rom::lookup_func(rom::code::reset_to_usb_boot))(0, 0);
    } else if(elms[0] == "version") {
        co_await print((const char*)rom::lookup_data(rom::code::copyright_string));
        co_await print("\n");
        co_ensure(co_await printf<"rom version {}\n">(ROM.version));
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

// clang may optimize loops below to __aeabi_memclr/__aeabi_memcpy,
// however rom::memcpy is not available yet.
__attribute__((optnone)) auto init_memory() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
}

auto entry() -> void {
    init_memory();
    M0PLUS_REGS.vector_table_offset = u32(usize(&vector[0])); // flash (boot2) or SRAM (bootloader), per link script
    rom::fops                       = (rom::FOps*)rom::lookup_data(rom::code::soft_float_table);
    if(ROM.version >= 2) {
        rom::dops = (rom::DOps*)rom::lookup_data(rom::code::soft_double_table);
    }
    const auto heap_end = (usize)&stack_top - 8 * 1024; // 8KB for stack
    noxx::set_heap(&heap_start, heap_end - (usize)&heap_start);
    enable_led();
    init_system();
    uart::init(921600);

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
    enable_led();
    while(true) {
        for(auto i = 0; i < 50000; i += 1) {
            led(true);
        }
        for(auto i = 0; i < 50000; i += 1) {
            led(false);
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
__attribute__((weak, alias("default_int_handler"))) auto uart_1_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto adc_irq_fifo_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto i2c_0_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto i2c_1_irq_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto rtc_irq_handler() -> void;

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
    (void*)&uart::uart0_handler,
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
