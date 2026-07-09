#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace hw::spi {
struct Control1 {
    enum : u32 {
        Enable      = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        MasterStart = 0b0000'0000'0000'0000'0000'0010'0000'0000, // with size=0, transfer runs until disabled
        SuspendReq  = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        InternalSS  = 0b0000'0000'0000'0000'0001'0000'0000'0000, // SS level when SoftSS, keep 1 in master
    };
};

struct Config1 {
    enum : u32 {
        DataSize      = 0b0000'0000'0000'0000'0000'0000'0001'1111, // bits per frame - 1
        FIFOThreshold = 0b0000'0000'0000'0000'0000'0001'1110'0000, // frames - 1
        TXDMAEnable   = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        RXDMAEnable   = 0b0000'0000'0000'0000'0100'0000'0000'0000,
        BaudRate      = 0b0111'0000'0000'0000'0000'0000'0000'0000, // kernel clock / 2^(n+1)
    };
};

struct Config2 {
    enum : u32 {
        Master        = 0b0000'0000'0100'0000'0000'0000'0000'0000,
        LSBFirst      = 0b0000'0000'1000'0000'0000'0000'0000'0000,
        ClockPhase    = 0b0000'0001'0000'0000'0000'0000'0000'0000,
        ClockPolarity = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        SoftSS        = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        KeepAF        = 0b1000'0000'0000'0000'0000'0000'0000'0000, // keep IO control while disabled
    };
};

struct Status {
    enum : u32 {
        RXPacket   = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        TXPacket   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        Underrun   = 0b0000'0000'0000'0000'0000'0000'0010'0000,
        Overrun    = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        ModeFault  = 0b0000'0000'0000'0000'0000'0010'0000'0000, // clears Enable and Master when raised
        TXComplete = 0b0000'0000'0000'0000'0001'0000'0000'0000,
    };
};

struct Regs {
    v32  control1;      // 0x00
    v32  control2;      // 0x04 [15:0] transfer size, 0 = endless
    v32  config1;       // 0x08
    v32  config2;       // 0x0C
    v32  int_enable;    // 0x10
    cv32 status;        // 0x14
    v32  int_clear;     // 0x18
    v32  autonomous;    // 0x1C
    v32  transmit_data; // 0x20 use byte access for 8-bit frames
    cv32 reserved0[3];  // 0x24
    cv32 receive_data;  // 0x30 use byte access for 8-bit frames
    cv32 reserved1[3];  // 0x34
    v32  crc_poly;      // 0x40
    cv32 tx_crc;        // 0x44
    cv32 rx_crc;        // 0x48
    v32  underrun_data; // 0x4C
};
static_assert(sizeof(Regs) == 0x4C + 4);
} // namespace hw::spi

#define SPI2_REGS (*(hw::spi::Regs*)(SPI2_BASE))
