#include <coop/platform.hpp>
#include <coop/timer.hpp>
#include <noxx/endian.hpp>

#include "arp.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net {
namespace {
// how often timer_task() ticks the protocol timers. arp retries at 1 s and ages
// at 60 s, so a coarse period keeps overhead low without missing deadlines.
constexpr auto timer_period_ms = u64(250);
} // namespace

auto NetIf::rx(AutoPacket packet) -> coop::Async<void> {
    if(packet->len < sizeof(EthernetHeader)) {
        co_return;
    }
    const auto header = *(const EthernetHeader*)packet->data();
    const auto src    = header.src;
    packet->consume(sizeof(EthernetHeader));
    switch(noxx::byteswap(header.ethertype)) {
    case EtherType::Arp:
        co_await arp::input(*stack, noxx::move(packet));
        break;
    case EtherType::IPv4:
        co_await ipv4::input(*stack, src, noxx::move(packet));
        break;
    default:
        break; // drop everything else
    }
}

auto Stack::timer_task() -> coop::Async<void> {
    while(true) {
        co_await coop::sleep_ms(timer_period_ms);
        co_await tick(coop::now_us() / 1000);
    }
}

auto Stack::init(NetIf& netif) -> void {
    this->netif = &netif;
    netif.stack = this;
}

auto Stack::tick(const u64 now_ms) -> coop::Async<void> {
    this->now_ms = now_ms;
    co_await arp::tick(*this, now_ms);
}

auto Stack::send(AutoPacket packet) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(netif != nullptr && netif->is_up());
    co_return co_await netif->tx(noxx::move(packet));
}

auto Stack::eth_send(const MacAddrRef dst, const u16 ethertype, AutoPacket packet) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(netif != nullptr);
    const auto raw = packet->prepend(sizeof(EthernetHeader));
    co_ensure(raw != nullptr, "no headroom for ethernet header");
    auto& header = *(EthernetHeader*)raw;
    noxx::memcpy(header.dst.data, dst.data, MacAddr::size());
    noxx::memcpy(header.src.data, netif->get_mac_addr().data, MacAddr::size());
    header.ethertype = noxx::byteswap(ethertype);
    co_return co_await send(noxx::move(packet));
}

auto Stack::on_link(const IPv4Addr ip) const -> bool {
    for(auto i = usize(0); i < IPv4Addr::size(); i += 1) {
        if((ip[i] & netmask[i]) != (addr[i] & netmask[i])) {
            return false;
        }
    }
    return true;
}
} // namespace net
