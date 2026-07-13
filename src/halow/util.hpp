#pragma once
#include <noxx/format.hpp>
#include <noxx/int.hpp>

// little-endian buffer accessors and console logging shared by the driver
namespace halow {
inline auto get_u16(const u8* const p) -> u16 {
    return u16(p[0]) | u16(p[1]) << 8;
}

inline auto get_u32(const u8* const p) -> u32 {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

inline auto put_u16(u8* const p, const u16 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
}

inline auto put_u32(u8* const p, const u32 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

template <noxx::comptime::String str, class... Args>
auto log(const Args&... args) -> void {
    auto raw = noxx::format<str>(noxx::move(args)...);
    if(raw) {
        noxx::console_out((*raw).data());
    }
}
} // namespace halow
