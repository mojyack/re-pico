#pragma once
#include <noxx/int.hpp>

namespace halow {
// sd crc7 (poly x^7+x^3+1), matches morselib morse_crc7_sd
constexpr auto crc7_step(u8 crc, const u8 byte) -> u8 {
    auto reg = u8((crc << 1) ^ byte);
    for(auto i = 0; i < 8; i += 1) {
        reg = (reg & 0x80) ? u8((reg << 1) ^ 0x12) : u8(reg << 1);
    }
    return reg >> 1;
}

constexpr auto crc7(const u8* const data, const usize size) -> u8 {
    auto crc = u8(0);
    for(auto i = usize(0); i < size; i += 1) {
        crc = crc7_step(crc, data[i]);
    }
    return crc;
}

// crc16 xmodem (poly 0x1021), matches morselib morse_crc16_xmodem
constexpr auto crc16_step(u16 crc, const u8 byte) -> u16 {
    crc ^= u16(byte) << 8;
    for(auto i = 0; i < 8; i += 1) {
        crc = (crc & 0x8000) ? u16((crc << 1) ^ 0x1021) : u16(crc << 1);
    }
    return crc;
}

constexpr auto crc16(const u8* const data, const usize size) -> u16 {
    auto crc = u16(0);
    for(auto i = usize(0); i < size; i += 1) {
        crc = crc16_step(crc, data[i]);
    }
    return crc;
}

constexpr u8 cmd0_frame[] = {0x40, 0, 0, 0, 0};
static_assert(crc7(cmd0_frame, 5) == 0x4a); // known CMD0 crc
} // namespace halow
