#pragma once
#include <crypto/util.hpp>
#include <noxx/array.hpp>
#include <noxx/optional.hpp>
#include <noxx/string-view.hpp>

namespace net {
bytes_alias(IPv4Addr, 4);

auto parse_ip(noxx::StringView str) -> noxx::Optional<IPv4Addr>;
} // namespace net
