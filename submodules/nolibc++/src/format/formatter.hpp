#pragma once
#include "../comptime-string.hpp"
#include "../string.hpp"

namespace noxx::fmt {
template <comptime::String params>
auto format(String& /*result*/, const auto& /*var*/) -> bool {
    static_assert(false, "no formatter implemented for that type");
    return false;
}
} // namespace noxx::fmt
