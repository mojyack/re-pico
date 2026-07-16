#pragma once
#include <coop/generator.hpp>
#include <net/netif.hpp>

namespace halow {
struct NetIf : net::NetIf {
    auto is_up() const -> bool override;
    auto get_mac_addr() const -> net::MacAddrRef override;
    auto tx(net::AutoPacket packet) -> coop::Async<bool> override;
};

inline auto netif = NetIf();
} // namespace halow
