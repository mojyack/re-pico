#pragma once
#include <noxx/int.hpp>
#include <noxx/span.hpp>

namespace rng {
// enable the hardware TRNG (kernel clock + peripheral). idempotent.
auto init() -> void;

// fill out with hardware random bytes. returns false on a persistent RNG fault
// (seed/clock error that does not clear after a reseed).
auto fill(noxx::Span<u8> out) -> bool;
} // namespace rng
