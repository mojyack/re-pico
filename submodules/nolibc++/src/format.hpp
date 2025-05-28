#pragma once
#include "comptime-string.hpp"
#include "format/boolean.hpp"
#include "format/integer.hpp"
#include "format/pointer.hpp"
#include "format/string.hpp"
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
    __builtin_memcpy(result.data() + prev_size, str.data + cursor, rest);
    return true;
}

template <comptime::String str, usize cursor, class Arg, class... Args>
auto format_step(String& result, const Arg& arg, const Args&... args) -> bool {
#define error_act return false
    constexpr auto open = find_not_escaped<str, '{', cursor>;
    static_assert(open != comptime::npos, "too many format parameters");

    const auto prev_size = result.size();
    const auto rest      = open - cursor;
    result.resize(prev_size + rest);
    __builtin_memcpy(result.data() + prev_size, str.data + cursor, rest);

    constexpr auto close = find_not_escaped<str, '}', open + 1>;
    static_assert(close != comptime::npos);
    constexpr auto fmt = comptime::substr<str, open + 1, close - (open + 1)>;
    ensure(format_segment<fmt>(result, arg));
    ensure((format_step<str, close + 1, Args...>(result, args...)));
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
