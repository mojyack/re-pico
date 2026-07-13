#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

namespace halow {
// firmware command ids (ref common/morse_commands.h)
struct CommandId {
    enum : u16 {
        GetVersion = 0x0002,
    };
};

// write a command to the yaps command queue and await its response.
// resp receives the response payload (after the status word); returns its size
auto send_command(u16 id, noxx::Span<const u8> req, noxx::Span<u8> resp) -> coop::Async<noxx::Optional<usize>>;

// fetch the next non-command from-chip frame, from the backlog or the chip;
// command-channel events and stray responses are logged and consumed.
// returns nullptr if nothing pending
auto fetch_rx() -> coop::Async<noxx::Optional<net::AutoPacket>>;
} // namespace halow
