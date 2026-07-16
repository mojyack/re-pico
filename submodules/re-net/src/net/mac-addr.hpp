#pragma once
#include <noxx/array.hpp>
#include <noxx/span.hpp>

#include <noxx/bytes-alias.hpp>

namespace net {
bytes_alias(MacAddr, 6);
} // namespace net

#include <noxx/bytes-alias.hpp>

// formatting support
#include <noxx/comptime-string.hpp>
#include <noxx/format/integer.hpp>
#include <noxx/string.hpp>

#include <noxx/assert.hpp>

namespace noxx::fmt {
template <comptime::String params>
auto format_segment(String& result, const net::MacAddrRef var) -> bool {
    constexpr auto error_value = false;
    static_assert(params.size() == 0, "format patameters not supported");
    for(auto i = 0uz; i < net::MacAddr::size(); i += 1) {
        ensure(format_segment<comptime::String("02x")>(result, var[i]));
        ensure(result.append(':'));
    }
    result.resize(result.size() - 1); // remove last ':'
    return true;
}
} // namespace noxx::fmt

#include <noxx/assert.hpp>
