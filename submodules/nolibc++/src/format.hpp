#pragma once
#include "comptime-string.hpp"
#include "format/formatter.hpp"
#include "format/integer.hpp"
#include "optional.hpp"

#include "assert.hpp"

namespace noxx {
namespace fmt {
template <comptime::String str, char key, usize index>
constexpr auto find_not_escaped_fn() -> usize {
    if constexpr(index >= str.size()) {
        return comptime::npos;
    } else if constexpr(str[index] == key) {
        if constexpr(index + 1 >= str.size() || str[index + 1] != key) {
            return index;
        } else {
            return find_not_escaped_fn<str, key, index + 2>();
        }
    } else {
        return find_not_escaped_fn<str, key, index + 1>();
    }
}
template <comptime::String str, char key, usize index = 0>
constexpr auto find_not_escaped = find_not_escaped_fn<str, key, index>();

template <comptime::String str, usize cursor>
auto format_step(String& result) -> bool {
    constexpr auto open = find_not_escaped<str, '{', cursor>;
    static_assert(open == comptime::npos, "not enough format parameters");

    constexpr auto rest      = str.size() - cursor;
    const auto     prev_size = result.size();
    result.resize(prev_size + rest);
    // printf("step(tail) result=%s cursor=%lu resize=%lu\n", result.data(), cursor, prev_size + rest);
    for(auto i = usize(0); i < rest; i += 1) {
        // printf("copy %d %c\n", i, str[cursor + i]);
        result[prev_size + i] = str[cursor + i];
    }
    return true;
}

template <comptime::String params, usize cursor, class Arg, class... Args>
auto format_step(String& result, const Arg& arg, const Args&... args) -> bool {
#define error_act return false
    constexpr auto open = find_not_escaped<params, '{', cursor>;
    static_assert(open != comptime::npos, "too many format parameters");

    const auto prev_size = result.size();
    const auto rest      = open - cursor;
    result.resize(prev_size + rest);
    // printf("step result=%s cursor=%lu open=%d resize=%lu\n", result.data(), cursor, (i64)open, prev_size + rest);
    for(auto i = usize(0); i < rest; i += 1) {
        result[prev_size + i] = params[cursor + i];
    }

    constexpr auto close = find_not_escaped<params, '}', open + 1>;
    static_assert(close != comptime::npos);
    constexpr auto fmt = comptime::substr<params, open + 1, close - (open + 1)>;
    ensure(format<fmt>(result, arg));
    ensure((format_step<params, close + 1, Args...>(result, args...)));
    return true;
#undef error_act
}
} // namespace fmt

template <comptime::String str, class... Args>
auto format(const Args&... args) -> Optional<String> {
#define error_act \
    return {}
    auto ret = String();
    ensure((fmt::format_step<str, 0, Args...>(ret, args...)));
    return ret;
#undef error_act
}
} // namespace noxx

#include "assert.hpp"
