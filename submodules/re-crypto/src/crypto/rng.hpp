#pragma once
#include <noxx/span.hpp>

namespace crypto {
struct Rng {
    virtual auto operator()(noxx::Span<u8> out) -> bool = 0;
};
} // namespace crypto
