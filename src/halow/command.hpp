#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

namespace halow {
// firmware command ids (ref common/morse_commands.h)
struct CommandId {
    enum : u16 {
        SetChannel      = 0x0001,
        GetVersion      = 0x0002,
        SetTxPower      = 0x0003,
        AddInterface    = 0x0004,
        RemoveInterface = 0x0005,
        BssConfig       = 0x0006,
        SetQosParams    = 0x0011,
        SetStaState     = 0x0014,
        ConfigPs        = 0x0016,
        HwScan          = 0x0044,
    };
};

// firmware event ids, arriving unsolicited on the command channel
struct EventId {
    enum : u16 {
        HwScanDone = 0x4011,
    };
};

// ADD_INTERFACE interface_type values (ref enum morse_cmd_interface_type)
struct InterfaceType {
    enum : u32 {
        Invalid = 0,
        Sta     = 1,
        Ap      = 2,
        Mon     = 3,
    };
};

constexpr auto vif_invalid = u16(0xffff);

// write a command to the yaps command queue and await its response.
// resp receives the response payload (after the status word); returns its size.
// resp_vif receives the vif_id field of the response header if non-null
auto send_command(u16 id, noxx::Span<const u8> req, noxx::Span<u8> resp, u16 vif = vif_invalid, u16* resp_vif = nullptr) -> coop::Async<noxx::Optional<usize>>;

// pop the oldest firmware event seen on the command channel:
// event id in the low half, first payload byte in the high half
auto pop_event() -> noxx::Optional<u32>;

inline auto event_id(const u32 event) -> u16 {
    return u16(event);
}

inline auto event_arg(const u32 event) -> u8 {
    return u8(event >> 16);
}

// fetch the next non-command from-chip frame, from the backlog or the chip;
// command-channel events and stray responses are logged and consumed.
// returns nullptr if nothing pending
auto fetch_rx() -> coop::Async<noxx::Optional<net::AutoPacket>>;
} // namespace halow
