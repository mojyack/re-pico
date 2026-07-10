#pragma once
#include <coop/io-pre.hpp>
#include <noxx/span.hpp>

namespace uart {
// non-blocking read/write; returns 0 when not ready
auto read(noxx::Span<u8> buf) -> usize;
auto write(noxx::Span<const u8> buf) -> usize;

// async facilities
inline auto read_event = coop::IOEvent();
} // namespace uart
