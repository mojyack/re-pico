#pragma once
#include "int.hpp"
#include "optional.hpp"
#include "string-view.hpp"
#include "type-traits.hpp"

#include "assert.hpp"

namespace noxx {
namespace charconv_impl {
inline auto digit_value(const char c) -> Optional<int> {
    constexpr auto error_value = nullopt;

    if(c >= '0' && c <= '9') {
        return c - '0';
    } else if(c >= 'a' && c <= 'z') {
        return c - 'a' + 10;
    } else if(c >= 'A' && c <= 'Z') {
        return c - 'A' + 10;
    }
    ensure(false);
}
} // namespace charconv_impl

template <Integral T>
auto from_chars(const StringView str, const int base = 10) -> Optional<T> {
    constexpr auto error_value = nullopt;
    constexpr auto is_signed   = T(-1) < T(0);

    ensure(base >= 2 && base <= 36);
    ensure(str.size() > 0);

    auto i        = usize(0);
    auto negative = false;
    if(str[i] == '+' || str[i] == '-') {
        negative = str[i] == '-';
        ensure(is_signed || !negative);
        i += 1;
    }
    ensure(i < str.size()); // at least one digit must follow the sign

    auto value = T(0);
    for(; i < str.size(); i += 1) {
        unwrap(digit, charconv_impl::digit_value(str[i]));
        ensure(digit < base);
        value = value * base + digit;
    }
    if(negative) {
        value = -value;
    }
    return value;
}
} // namespace noxx

#include "assert.hpp"
