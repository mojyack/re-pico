#pragma once
#include <coop/generator.hpp>
#include <coop/ext-event-pre.hpp>
#include <noxx/int.hpp>
#include <noxx/optional.hpp>

// MM8108 HaLow radio, connected to SPI (SDIO-over-SPI protocol)
namespace halow {
// chip registers
struct Reg {
    enum : u32 {
        ChipID  = 0x2d20, // 0x0609/0x0709/0x0809 = MM8108 B0/B1/B2
        Int1Sts = 0x3c50, // host interrupt block (ref morse_driver hw.h)
        Int1Set = 0x3c54,
        Int1Clr = 0x3c58,
        Int1En  = 0x3c5c,
        Int2Sts = 0x3c60,
        Int2Set = 0x3c64,
        Int2Clr = 0x3c68,
        Int2En  = 0x3c6c,
    };
};

// Int1Sts/Int1En bits (ref mm8108/yaps-hw.h)
struct Int1 {
    enum : u32 {
        FcPktWaiting = 1 << 0, // a from-chip frame is queued on the yaps stream
        FcPageFreed  = 1 << 1, // to-chip pool pages were released
    };
};

// the chip holds its irq line low while any enabled Int1Sts bit is set; the
// platform's edge interrupt handler calls notify(), available() samples the
// level so a still-asserted line is never missed between edges
struct ChipIrqEvent : coop::ExtEvent {
    auto available() const -> bool override;
};
inline auto chip_irq_event = ChipIrqEvent();

// reset the chip and establish sdio-over-spi communication
auto init() -> coop::Async<bool>;
auto read_u32(u32 address) -> noxx::Optional<u32>;
auto write_u32(u32 address, u32 value) -> bool;
auto read_multi(u32 address, u8* data, u32 size) -> bool;
auto write_multi(u32 address, const u8* data, u32 size) -> bool;
} // namespace halow
