#pragma once
#include <coop/generator.hpp>
#include <noxx/span.hpp>

namespace uart {
auto read_blocking(noxx::Span<u8> buf) -> void;
auto write_blocking(noxx::Span<const u8> buf) -> void;
auto read_all(noxx::Span<u8> buf) -> coop::Async<void>;
auto write_all(noxx::Span<const u8> buf) -> coop::Async<void>;
} // namespace uart
