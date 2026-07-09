#pragma once
#include <noxx/int.hpp>
#include <noxx/optional.hpp>

// MM8108 HaLow radio, connected to SPI (SDIO-over-SPI protocol)
namespace halow {
// chip registers
struct Reg {
    enum : u32 {
        ChipID = 0x2d20, // 0x0609/0x0709/0x0809 = MM8108 B0/B1/B2
    };
};

// reset the chip and establish sdio-over-spi communication
auto init() -> bool;
auto read_u32(u32 address) -> noxx::Optional<u32>;
auto write_u32(u32 address, u32 value) -> bool;
auto write_multi(u32 address, const u8* data, u32 size) -> bool;
} // namespace halow
