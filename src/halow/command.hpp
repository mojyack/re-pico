#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

#include "commands.hpp"

// firmware command transport over the yaps command channel
namespace halow {
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

// from-chip pump, started once after init_yaps: sleeps on the chip irq line,
// acks the interrupt and demultiplexes the yaps stream — command responses to
// the sender, events to the ring, beacons/tx-status dropped, everything else
// to the rx backlog (consumed via wait_rx/pop_rx_backlog). returns only on an
// unrecoverable rx stream desync
auto rx_task() -> coop::Async<bool>;

// true if rx_task gave up on a wedged rx stream; the chip must be rebooted
auto rx_desynced() -> bool;
} // namespace halow
