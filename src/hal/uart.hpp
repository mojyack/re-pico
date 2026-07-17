#pragma once
#include <coop/ext-event-pre.hpp>
#include <noxx/span.hpp>

namespace uart {
// non-blocking read/write; returns 0 when not ready
auto read(noxx::Span<u8> buf) -> usize;
auto write(noxx::Span<const u8> buf) -> usize;

// async facilities
auto read_available() -> bool;
auto write_available() -> bool;

struct ReadEvent : coop::ExtEvent {
    auto available() const -> bool override {
        return read_available();
    }
};

struct WriteEvent : coop::ExtEvent {
    auto available() const -> bool override {
        return write_available();
    }
};

inline auto read_event  = ReadEvent();
inline auto write_event = WriteEvent();
} // namespace uart
