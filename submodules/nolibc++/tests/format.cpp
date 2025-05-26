#include <print>

#include "noxx/format.hpp"
#include "noxx/malloc.hpp"

#include "noxx/assert.hpp"

namespace {
auto heap = std::array<std::byte, 4096>();
}

auto main() -> int {
#define error_act return -1
    static_assert(noxx::comptime::str_to_int<"128"> == 128);
    static_assert(noxx::comptime::str_to_int<"-128"> == -128);
    static_assert(noxx::comptime::str_to_int<"ff", 16> == 255);
    noxx::set_heap(heap.data(), heap.size());
    // plain string
    ensure(*noxx::format<"hello">() == "hello");
    ensure(*noxx::format<"{} {}">("hello", "world") == "hello world");
    // escape
    ensure(*noxx::format<"{{}}">() == "{{}}");

    // integer
    ensure(*noxx::format<"u8={}">(u8(0xff)) == "u8=255");
    ensure(*noxx::format<"u16={}">(u16(0x7fff)) == "u16=32767");
    ensure(*noxx::format<"i16={}">(i16(-0x7fff)) == "i16=-32767");
    ensure(*noxx::format<"u32={}">(u32(0x7fffffff)) == "u32=2147483647");
    ensure(*noxx::format<"i32={}">(i32(-0x7fffffff)) == "i32=-2147483647");
    ensure(*noxx::format<"u64={}">(u64(0x7fffffffffffffff)) == "u64=9223372036854775807");
    ensure(*noxx::format<"i64={}">(i64(-0x7fffffffffffffff)) == "i64=-9223372036854775807");
    // format parameters
    ensure(*noxx::format<"u8={x}">(u8(0xff)) == "u8=ff");
    ensure(*noxx::format<"u8={02x}">(u8(0xff)) == "u8=ff");
    ensure(*noxx::format<"u8={03x}">(u8(0xff)) == "u8=0ff");

    // boolean
    ensure(*noxx::format<"true={} false={}">(true, false) == "true=true false=false");

    std::println("pass");
    return 0;
#undef error_act
}
