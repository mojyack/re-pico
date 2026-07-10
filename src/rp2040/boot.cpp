#include <inflate.hpp>
#include <noxx/array.hpp>
#include <print.hpp>
#include <uart.hpp>

#include "hal/time.hpp"
#include "hal/uart.hpp"
#include "hw/m0plus.hpp"
#include "hw/nvic.hpp"
#include "hw/uart.hpp"
#include "system.hpp"

#include <noxx/assert.hpp>

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
constexpr auto fw_load_size  = usize(192 * 1024);
constexpr auto comp_buf_base = usize(0x20030000);
constexpr auto comp_buf_size = usize(48 * 1024);
static_assert(fw_load_base + fw_load_size == comp_buf_base);
static_assert(comp_buf_base + comp_buf_size == 0x2003c000); // = ORIGIN(sram) of link-boot.ld

auto get_u32() -> u32 {
    auto buf = noxx::Array<u8, 4>();
    uart::read_blocking(buf);
    return u32(buf[0]) << 0 | u32(buf[1]) << 8 | u32(buf[2]) << 16 | u32(buf[3]) << 24;
}

auto get_u8() -> u8 {
    auto buf = noxx::Array<u8, 1>();
    uart::read_blocking(buf);
    return buf[0];
}

auto crc32(noxx::Span<u8> buf) -> u32 {
    auto crc = u32(0xffffffff);
    for(auto i = u32(0); i < buf.size; i += 1) {
        crc ^= buf[i];
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
        const auto c = get_u8();
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

// clang may optimize the loops below to __aeabi_memclr/__aeabi_memcpy,
// however rom::memcpy is not available yet.
__attribute__((optnone)) auto init_memory() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
}

[[noreturn]] auto entry() -> void {
    init_memory();
    M0PLUS_REGS.vector_table_offset = u32(usize(&vector[0]));
    enable_led();
    init_system();
    uart::init(921600);

    while(true) {
#pragma push_macro("error_act")
#define error_act                    \
    for(auto k = 0; k < 6; k += 1) { \
        led(k % 2 == 0);             \
        time::delay(80'000);         \
    }                                \
    continue;

        led(false); // ready
        print_blocking("\nbootloader\n");

        wait_for_magic();
        led(true); // receiving
        const auto comp_len = get_u32();
        const auto orig_len = get_u32();
        const auto want_crc = get_u32();
        ensure(comp_len <= comp_buf_size && orig_len <= fw_load_size, "image too large");
        auto comp = noxx::Span<u8>{(u8*)comp_buf_base, comp_len};
        uart::read_blocking(comp);
        ensure(crc32(comp) == want_crc, "crc mismatch");
        print_blocking("decompressing\n");
        ensure(inflate(comp, {(u8*)fw_load_base, orig_len}), "inflate failed");
        print_blocking("jumping to firmware\n");
        // drain the transmitter before tearing down the uart
        while(!(UART0_REGS.flag & uart::Flag::TXFIFOEmpty) || (UART0_REGS.flag & uart::Flag::Busy)) {
        }
        uart::deinit();
        launch(fw_load_base);
#pragma pop_macro("error_act")
    }
}
} // namespace

extern "C" {
[[noreturn]] auto default_int_handler() -> void {
    // fast blink signals a fault in the bootloader
    enable_led();
    while(true) {
        for(auto i = 0; i < 300000; i += 1) {
            led(true);
        }
        for(auto i = 0; i < 300000; i += 1) {
            led(false);
        }
    }
}

__attribute__((weak, alias("default_int_handler"))) auto nmi_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto hard_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pend_sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto systick_handler() -> void;

__attribute__((section(".vector"))) void* vector[16 + u32(nvic::IRQ::Uart0) + 1] = {
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
    [16 + u32(nvic::IRQ::Uart0)] = (void*)&uart::uart0_handler,
};
}
