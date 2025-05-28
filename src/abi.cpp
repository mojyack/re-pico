#include "noxx/bits.hpp"

namespace {
constexpr auto udiv(uint numerator, uint denominator) -> uint {
    auto quotient = uint(0);
    for(auto i = int(noxx::msb(denominator)); i >= 0 && numerator >= denominator; i -= 1) {
        const auto u = denominator << i;
        if(numerator >= u) {
            numerator -= u;
            quotient += uint(1) << i;
        }
    }
    return quotient;
}
static_assert(udiv(8, 4) == 2);
static_assert(udiv(9, 3) == 3);
static_assert(udiv(1, 2) == 0);
static_assert(udiv(128, 2) == 64);
} // namespace

extern "C" {
auto __aeabi_uidiv(uint numerator, uint denominator) -> uint {
    return udiv(numerator, denominator);
}
}
