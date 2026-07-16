#pragma once
#include <noxx/array.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

#include <noxx/bytes-alias.hpp>

namespace net {
bytes_alias(IPv4Addr, 4);

auto parse_ip(noxx::StringView str) -> noxx::Optional<IPv4Addr>;
} // namespace net

#include <noxx/bytes-alias.hpp>
