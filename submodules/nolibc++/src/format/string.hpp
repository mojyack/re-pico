#pragma once
#include "../comptime-string.hpp"
#include "../string-view.hpp"
#include "../string.hpp"

#include "../assert.hpp"

namespace noxx::fmt {
template <comptime::String params>
auto format_segment(String& result, const StringView& var) -> bool {
    constexpr auto error_value = false;
    static_assert(params.size() == 0, "format patameters not supported");
    ensure(result.append(var));
    return true;
}

template <comptime::String params>
auto format_segment(String& result, const char* var) -> bool {
    return format_segment<params>(result, StringView(var));
}
} // namespace noxx::fmt

#include "../assert.hpp"
