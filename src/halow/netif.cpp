#include <coop/promise.hpp>
#include <net/ethernet.hpp>

#include "connect.hpp"
#include "netif.hpp"

#include <noxx/assert.hpp>
#include <noxx/endian.hpp>

namespace halow {
auto NetIf::is_up() const -> bool {
    return link_status().up;
}

auto NetIf::get_mac_addr() const -> net::MacAddrRef {
    return link_status().mac;
}

auto NetIf::tx(net::AutoPacket packet) -> coop::Async<bool> {
    // net::Stack prepends a full ethernet header; halow::eth_tx wants the fields
    // split out and rebuilds the 802.11 header itself, so peel it back off here
    constexpr auto error_value = false;

    co_ensure(packet->len >= sizeof(net::EthernetHeader), "short tx frame");
    const auto& eth       = *(const net::EthernetHeader*)packet->data();
    const auto  dst       = eth.dst;
    const auto  ethertype = noxx::byteswap(eth.ethertype);
    packet->consume(sizeof(net::EthernetHeader));
    co_return co_await eth_tx(dst, ethertype, {packet->data(), packet->len});
}
} // namespace halow
