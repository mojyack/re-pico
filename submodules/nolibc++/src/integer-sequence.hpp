#pragma once
#include "type-traits.hpp"

namespace noxx {
template <Integral T, T... N>
struct IntegerSequence {
};

template <class T, T N>
using make_integer_sequence = __make_integer_seq<IntegerSequence, T, N>;
} // namespace noxx
