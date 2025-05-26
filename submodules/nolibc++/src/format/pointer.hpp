#pragma once
#include "integer.hpp"

#include "../assert.hpp"

namespace noxx::fmt {
template <comptime::String params, class T>
auto format_segment(String& result, const T* var) -> bool {
#define error_act return false
    static_assert(params.size() == 0, "format patameters not supported");
    constexpr auto inner_params = comptime::concat<"0", comptime::int_to_str<sizeof(usize) * 2>, "x">;
    ensure(result.append("0x"));
    ensure(format_segment<inner_params>(result, (usize)var));
    return true;
#undef error_act
}
} // namespace noxx::fmt

#include "../assert.hpp"
