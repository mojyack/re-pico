#pragma once
#include <noxx/optional.hpp>

namespace halow {
struct FirmwareInfo {
    u32 host_table_ptr;
    u32 magic;
    u32 version;
};

constexpr auto host_magic = u32(0xDEADBEEF);

constexpr auto version_major(const u32 v) -> u32 {
    return (v >> 22) & 0x3ff;
}
constexpr auto version_minor(const u32 v) -> u32 {
    return (v >> 10) & 0xfff;
}
constexpr auto version_patch(const u32 v) -> u32 {
    return v & 0x3ff;
}

auto load_firmware() -> noxx::Optional<FirmwareInfo>;
} // namespace halow
