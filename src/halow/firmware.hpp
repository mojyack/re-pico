#pragma once
#include <coop/generator.hpp>
#include <noxx/optional.hpp>

#include "fw-store.hpp"

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

auto load_firmware(const FwStore& store) -> coop::Async<noxx::Optional<FirmwareInfo>>;

// the store passed to the last load_firmware, nullptr before the first load
extern const FwStore* fw_store;

// read back the host table pointer published by a booted firmware
auto host_table_ptr() -> noxx::Optional<u32>;
} // namespace halow
