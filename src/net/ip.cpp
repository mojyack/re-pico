#include <net/ip.hpp>
#include <noxx/array.hpp>
#include <noxx/charconv.hpp>

#include <noxx/assert.hpp>
#include <split.hpp>

namespace net {
auto parse_ip(const noxx::StringView str) -> noxx::Optional<IPv4Addr> {
    constexpr auto error_value = noxx::nullopt;
    unwrap(parts, split(str, "."));
    ensure(parts.size() == 4, "bad ipv4 address");

    auto ret = IPv4Addr();
    for(auto i = usize(0); i < 4; i += 1) {
        unwrap(v, noxx::from_chars<u8>(parts[i]));
        ret[i] = v;
    }
    return ret;
};
} // namespace net
