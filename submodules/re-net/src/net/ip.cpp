#include <noxx/charconv.hpp>

#include "ip.hpp"

#include <noxx/assert.hpp>

namespace net {
auto parse_ip(const noxx::StringView str) -> noxx::Optional<IPv4Addr> {
    constexpr auto error_value = noxx::nullopt;

    auto ret  = IPv4Addr();
    auto rest = str;
    for(auto i = usize(0); i < 4; i += 1) {
        const auto last = i == 3;
        const auto dot  = rest.find(".");
        ensure(last ? dot < 0 : dot >= 0, "bad ipv4 address");
        unwrap(v, noxx::from_chars<u8>(last ? rest : rest.substr(0, dot)));
        ret[i] = v;
        if(!last) {
            rest = rest.substr(dot + 1);
        }
    }
    return ret;
}
} // namespace net
