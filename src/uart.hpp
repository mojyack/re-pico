#pragma once
#include <noxx/span.hpp>

namespace uart {
auto read_all(noxx::Span<u8> buf) -> void;
auto write_all(noxx::Span<const u8> buf) -> void;
} // namespace uart
