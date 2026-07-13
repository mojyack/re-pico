#pragma once
#include <coop/generator.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

#include "host-table.hpp"

namespace halow {
// firmware command ids (ref common/morse_commands.h)
struct CommandId {
    enum : u16 {
        GetVersion = 0x0002,
    };
};

// remember the yaps stream layout, must be called once after parse_host_table
auto init_command(const YapsTable& yaps) -> void;

// write a command to the yaps command queue and await its response.
// resp receives the response payload (after the status word); returns its size
auto send_command(u16 id, noxx::Span<const u8> req, noxx::Span<u8> resp) -> coop::Async<noxx::Optional<usize>>;

// drain one pending from-chip packet, if any; events are only logged for now.
// returns true if a packet was consumed
auto poll_rx() -> coop::Async<noxx::Optional<bool>>;
} // namespace halow
