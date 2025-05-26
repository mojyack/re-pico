#pragma once
#include "../array.hpp"
#include "../comptime-string.hpp"
#include "../int.hpp"
#include "../math.hpp"
#include "../string-view.hpp"
#include "../string.hpp"
#include "../type-traits.hpp"

#include "../assert.hpp"

namespace noxx::fmt {
namespace integer {
template <class T>
concept format_supported_integrals = is_same<T, u8> ||
                                     is_same<T, i16> || is_same<T, u16> ||
                                     is_same<T, i32> || is_same<T, u32> ||
                                     is_same<T, i64> || is_same<T, u64>;

struct Params {
    char filler;
    int  width;
    int  base;
};

template <char c>
constexpr auto base_char_to_base() -> int {
    if constexpr(c == 'd') {
        return 10;
    } else if constexpr(c == 'x') {
        return 16;
    } else if constexpr(c == 'b') {
        return 2;
    } else {
        static_assert(false, "unsupported base");
    }
}

template <comptime::String params>
constexpr auto parse_params() -> Params {
    if constexpr(params.size() == 0) {
        return Params{' ', 0, 10};
    } else if constexpr(params.size() == 1) {
        return Params{' ', 0, base_char_to_base<params[0]>()};
    } else {
        constexpr auto filler = params[0];
        constexpr auto width  = comptime::str_to_int<comptime::substr<params, 1, params.size() - 2>>;
        constexpr auto base   = base_char_to_base<params[-1]>();
        return Params{filler, width, base};
    }
}
} // namespace integer

// params={} or {(base)} or {(filler,width,base)} where base=one of "dxb"
template <comptime::String params>
auto format(String& result, const integer::format_supported_integrals auto& var) -> bool {
#define error_act return false
    constexpr auto ps = integer::parse_params<params>();

    constexpr auto max_int_len = log((uintn<sizeof(int) * 8>)-1, ps.base) + 1 /*sign*/;

    auto       buf    = Array<char, max(max_int_len, ps.width)>();
    auto       cursor = buf.size() - 1;
    auto       num    = var >= 0 ? var : -var;
    const auto put    = [&](char c) {
        buf[cursor] = c;
        cursor -= 1;
    };
    do {
        put(comptime::char_table[num % ps.base]);
        num /= ps.base;
    } while(num != 0);
    if(var < 0) {
        put('-');
    }
    while(buf.size() - cursor - 1 < ps.width) {
        put(ps.filler);
    }
    ensure(result.append(StringView(buf.data + cursor + 1, buf.size() - cursor - 1)));
    return true;
#undef error_act
}
} // namespace noxx::fmt

#include "../assert.hpp"
