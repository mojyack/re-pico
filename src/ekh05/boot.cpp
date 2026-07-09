#include <inflate.hpp>
#include <noxx/array.hpp>
#include <noxx/assert.hpp>
#include <noxx/bits.hpp>

#include "hw/gpio.hpp"
#include "hw/m33.hpp"
#include "hw/usart.hpp"
#include "system.hpp"

// from link-boot.ld
extern "C" void* vector[];
extern u32       bss_start;
extern u32       bss_end;
extern u32       data_start;
extern u32       data_end;
extern u32       data_load;
extern u32       stack_top;

namespace {
constexpr auto fw_load_base  = usize(0x20000000);
constexpr auto fw_load_size  = usize(448 * 1024);
constexpr auto comp_buf_base = usize(0x20070000);
constexpr auto comp_buf_size = usize(288 * 1024);
static_assert(fw_load_base + fw_load_size == comp_buf_base);
static_assert(comp_buf_base + comp_buf_size == 0x200b8000); // = ORIGIN(sram) of link-boot.ld

auto get_u32() -> u32 {
    const auto b0 = u32(uart_getc());
    const auto b1 = u32(uart_getc());
    const auto b2 = u32(uart_getc());
    const auto b3 = u32(uart_getc());
    return b0 | b1 << 8 | b2 << 16 | b3 << 24; // little-endian
}

auto crc32(const u8* data, const u32 len) -> u32 {
    auto crc = u32(0xffffffff);
    for(auto i = u32(0); i < len; i += 1) {
        crc ^= data[i];
        for(auto k = u32(0); k < 8; k += 1) {
            crc = (crc >> 1) ^ (u32(0xedb88320) & (~(crc & 1) + 1)); // reflected CRC-32 poly
        }
    }
    return ~crc;
}

// block until the 4-byte frame magic "RPBL" arrives
auto wait_for_magic() -> void {
    constexpr auto magic = noxx::Array<u8, 4>{'R', 'P', 'B', 'L'};

    auto matched = u32(0);
    while(matched < 4) {
        const auto c = uart_getc();
        if(c == magic[matched]) {
            matched += 1;
        } else {
            matched = c == magic[0] ? 1 : 0;
        }
    }
}

[[noreturn]] auto launch(const usize base) -> void {
    const auto sp = ((u32*)base)[0];
    const auto pc = ((u32*)base)[1];
    asm volatile("msr msp, %0\n"
                 "bx %1\n"
                 :
                 : "r"(sp), "r"(pc)
                 : "memory");
    __builtin_unreachable();
}

[[noreturn]] auto entry() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
    SCB_REGS.vector_table_offset = u32(usize(&vector[0]));
    enable_leds();
    init_system();
    init_uart(921600);

    while(true) {
#pragma push_macro("error_act");
#define error_act       \
    led(led_red, true); \
    continue;

        led(led_red, false);
        led(led_blue, false);
        led(led_green, true); // ready
        println("\r\nbootloader");

        wait_for_magic();
        led(led_green, false);
        led(led_blue, true); // receiving
        const auto comp_len = get_u32();
        const auto orig_len = get_u32();
        const auto want_crc = get_u32();
        ensure(comp_len <= comp_buf_size && orig_len <= fw_load_size, "image too large");
        const auto comp = (u8*)comp_buf_base;
        auto       got  = u32(0);
        for(; got < comp_len; got += 1) {
            auto spins = u32(0);
            while(!(LPUART1_REGS.status & hw::usart::Status::RXNotEmpty)) {
                if(LPUART1_REGS.status & hw::usart::Status::OverrunError) {
                    LPUART1_REGS.int_clear = hw::usart::Status::OverrunError;
                }
                spins += 1;
                if(spins > 30'000'000) {
                    goto recv_done;
                }
            }
            comp[got] = u8(LPUART1_REGS.receive_data);
        }
    recv_done:
        led(led_blue, false);
        ensure(got == comp_len, "short receive");
        ensure(crc32(comp, comp_len) == want_crc, "crc mismatch");
        ensure(inflate({comp, comp_len}, {(u8*)fw_load_base, orig_len}), "inflate failed");
        println("jumping to firmware");
        wait_for_bit(LPUART1_REGS.status, hw::usart::Status::TXComplete);
        launch(fw_load_base);
#pragma pop_macro("error_act")
    }
}
} // namespace

extern "C" {
[[noreturn]] auto default_int_handler() -> void {
    // fast red blink signals a fault in the bootloader
    while(true) {
        for(auto i = 0; i < 300000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << led_red;
        }
        for(auto i = 0; i < 300000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << (led_red + 16);
        }
    }
}

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
