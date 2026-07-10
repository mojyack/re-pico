#pragma once
#include <coop/io-pre.hpp>
#include <noxx/span.hpp>

namespace uart {
// non-blocking read/write; returns 0 when not ready
auto read(noxx::Span<u8> buf) -> usize;
auto write(noxx::Span<const u8> buf) -> usize;

// async facilities
auto read_available() -> bool;
auto write_available() -> bool;

struct ReadIOEvent : coop::IOEvent {
    auto available() const -> bool override {
        return read_available();
    }
};

struct WriteIOEvent : coop::IOEvent {
    auto available() const -> bool override {
        return write_available();
    }
};

inline auto read_event  = ReadIOEvent();
inline auto write_event = WriteIOEvent();
} // namespace uart
