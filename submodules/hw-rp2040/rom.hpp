#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace rom {
struct Layout {
    u32 initial_stack;
    u32 reset_handler;
    u32 nmi_handler;
    u32 hard_fault_handler;
    u8  magic[3];
    u8  version;
    u16 func_table;
    u16 data_table;
    u16 table_lookup;
};

namespace code {
constexpr auto code(const char (&c)[3]) -> u32 {
    return c[1] << 8 | c[0];
}

// function codes

// bits
constexpr auto popcount32 = code("P3");
constexpr auto reverse32  = code("R3");
constexpr auto clz32      = code("R3"); // 0b00010 -> 3 0b00000 -> 32
constexpr auto ctz32      = code("T3"); // 0b00010 -> 1 0b00000 -> 32
// memory
constexpr auto memset   = code("MS");
constexpr auto memset4  = code("S4"); // word aligned fast memset
constexpr auto memcpy   = code("MC");
constexpr auto memcpy44 = code("C4"); // word aligned fast memcpy
// flash
constexpr auto connect_internal_flash = code("IF");
constexpr auto flash_exit_xip         = code("EX");
constexpr auto flash_range_program    = code("RP");
constexpr auto flash_flush_cache      = code("FC");
constexpr auto flash_enter_cmd_xip    = code("CX");
// debug
constexpr auto debug_trampoline     = code("DT");
constexpr auto debug_trampoline_end = code("DE");
// misc
constexpr auto reset_to_usb_boot = code("UB");
constexpr auto wait_for_vector   = code("WV");

// data codes

constexpr auto copyright_string  = code("CR");
constexpr auto git_version       = code("GR");
constexpr auto fplib_start       = code("FS");
constexpr auto soft_float_table  = code("SF");
constexpr auto fplib_end         = code("FE");
constexpr auto soft_double_table = code("SD"); // v2
} // namespace code

#define ROM (*(rom::Layout*)(ROM_BASE))

__attribute__((always_inline)) inline auto lookup_func(const u32 code) -> void* {
    using lookup_table_fn   = void*(u16 * table, u32 code);
    const auto lookup_table = reinterpret_cast<lookup_table_fn*>(u32(ROM.table_lookup));
    const auto func_table   = reinterpret_cast<u16*>(u32(ROM.func_table));
    return lookup_table(func_table, code);
}

__attribute__((always_inline)) inline auto lookup_data(const u32 code) -> void* {
    using lookup_table_fn   = void*(u16 * table, u32 code);
    const auto lookup_table = reinterpret_cast<lookup_table_fn*>(u32(ROM.table_lookup));
    const auto data_table   = reinterpret_cast<u16*>(u32(ROM.data_table));
    return lookup_table(data_table, code);
}

// pointer cache available
extern u32 (*popcount32)(u32 value);
extern u32 (*reverse32)(u32 value);
extern u32 (*clz32)(u32 value);
extern u32 (*ctz32)(u32 value);
extern u8* (*memset)(u8* ptr, u8 c, u32 n);
extern u8* (*memset4)(u8* ptr, u8 c, u32 n);
extern u8* (*memcpy)(u8* dest, u8* src, u32 n);
extern u8* (*memcpy44)(u8* dest, u8* src, u32 n);

// only types
using connect_internal_flash = void(void);
using flash_exit_xip         = void(void);
using flash_range_erase      = void(u32 addr, usize count, u32 block_size, u8 block_cmd);
using flash_range_program    = void(u32 addr, const u8* data, usize count);
using flash_flush_cache      = void(void);
using flash_enter_cmd_xip    = void(void);
using reset_to_usb_boot      = void(u32 gpio_activity_pin_mask, u32 disable_interface_mask);

// floating point functions
struct FOps {
    float (*fadd)(float a, float b);
    float (*fsub)(float a, float b);
    float (*fmul)(float a, float b);
    float (*fdiv)(float a, float b);
    void* deprecated1;
    void* deprecated2;
    float (*fsqrt)(float v);
    int (*float2int)(float v);
    int (*float2fix)(float v, int n);
    uint (*float2uint)(float v);
    uint (*float2ufix)(float v, uint n);
    float (*int2float)(int v);
    float (*fix2float)(int v, int n);
    float (*uint2float)(uint v);
    float (*ufix2float)(uint v, int n);
    float (*fcos)(float angle);
    float (*fsin)(float angle);
    float (*ftan)(float angle);
    float (*fsincos)(float angle); // v3
    float (*fexp)(float v);
    float (*fln)(float v);
    int (*fcmp)(float a, float b);        // v2
    float (*fatan2)(float y, float x);    // v2
    float (*int642float)(i64 v);          // v2
    float (*fix642float)(i64 v, int n);   // v2
    float (*uint642float)(u64 v);         // v2
    float (*ufix642float)(u64 v, int n);  // v2
    i64 (*float2int64)(float v);          // v2
    i64 (*float2fix64)(float v, int n);   // v2
    u64 (*float2uint64)(float v);         // v2
    u64 (*float2ufix64)(float v, uint n); // v2
    double (*float2double)(float v);      // v2
};

struct DOps {
    double (*dadd)(double a, double b);
    double (*dsub)(double a, double b);
    double (*dmul)(double a, double b);
    double (*ddiv)(double a, double b);
    void* deprecated1;
    void* deprecated2;
    double (*fsqrt)(double v);
    int (*double2int)(double v);
    int (*double2fix)(double v, int n);
    uint (*double2uint)(double v);
    uint (*double2ufix)(double v, uint n);
    double (*int2double)(int v);
    double (*fix2double)(int v, int n);
    double (*uint2double)(uint v);
    double (*ufix2double)(uint v, int n);
    double (*dcos)(double angle);
    double (*dsin)(double angle);
    double (*dtan)(double angle);
    double (*dsincos)(double angle); // v3
    double (*dexp)(double v);
    double (*dln)(double v);
    int (*dcmp)(double a, double b);
    double (*datan2)(double y, double x);
    double (*int642double)(i64 v);
    double (*fix642double)(i64 v, int n);
    double (*uint642double)(u64 v);
    double (*ufix642double)(u64 v, int n);
    i64 (*double2int64)(double v);
    i64 (*double2fix64)(double v, int n);
    u64 (*double2uint64)(double v);
    u64 (*double2ufix64)(double v, uint n);
    float (*double2float)(double v);
};

inline auto fops = (const FOps*)(nullptr);
inline auto dops = (const DOps*)(nullptr);
} // namespace rom
