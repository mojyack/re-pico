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

auto Stack::on_rx(NetIf& netif, AutoPacket packet) -> void {
    // enqueue and wake run(); the frame is demuxed later on its loop
    auto&      self = *(Stack*)netif.stack;
    const auto raw  = packet.release();
    raw->next       = nullptr;
    if(self.rx_tail == nullptr) {
        self.rx_head = raw;
    } else {
        self.rx_tail->next = raw;
    }
    self.rx_tail = raw;
    self.rx_event.notify();
}

auto Stack::run() -> coop::Async<void> {
    while(true) {
        while(dispatch()) {
        }
        co_await rx_event;
    }
}

auto Stack::timer_task() -> coop::Async<void> {
    while(true) {
        co_await coop::sleep_ms(timer_period_ms);
        tick(coop::now_us() / 1000);
    }
}

auto Stack::init(NetIf& netif) -> void {
    this->netif = &netif;
    netif.stack = this;
    netif.rx    = on_rx;
}

auto Stack::dispatch() -> bool {
    if(rx_head == nullptr) {
        return false;
    }
    auto packet = AutoPacket(rx_head);
    rx_head     = packet->next;
    if(rx_head == nullptr) {
        rx_tail = nullptr;
    }
    packet->next = nullptr;
    process(noxx::move(packet));
    return true;
}

auto Stack::tick(const u64 now_ms) -> void {
    this->now_ms = now_ms;
    arp::tick(*this, now_ms);
}

auto Stack::send(AutoPacket packet) -> bool {
    constexpr auto error_value = false;

    ensure(netif != nullptr && netif->tx != nullptr && netif->link_up);
    return netif->tx(*netif, noxx::move(packet));
}

auto Stack::eth_send(const MacAddrRef dst, const u16 ethertype, AutoPacket packet) -> bool {
    constexpr auto error_value = false;

    ensure(netif != nullptr);
    const auto raw = packet->prepend(sizeof(EthernetHeader));
    ensure(raw != nullptr, "no headroom for ethernet header");
    auto& header = *(EthernetHeader*)raw;
    noxx::memcpy(header.dst.data, dst.data, MacAddr::size());
    noxx::memcpy(header.src.data, netif->mac.data, MacAddr::size());
    header.ethertype = noxx::byteswap(ethertype);
    return send(noxx::move(packet));
}

auto Stack::on_link(const IPv4Addr ip) const -> bool {
    for(auto i = usize(0); i < IPv4Addr::size(); i += 1) {
        if((ip[i] & netmask[i]) != (addr[i] & netmask[i])) {
            return false;
        }
    }
    return true;
}

auto Stack::process(AutoPacket packet) -> void {
    if(packet->len < sizeof(EthernetHeader)) {
        return;
    }
    const auto header = *(const EthernetHeader*)packet->data();
    const auto src    = header.src;
    packet->consume(sizeof(EthernetHeader));
    switch(noxx::byteswap(header.ethertype)) {
    case EtherType::Arp:
        arp::input(*this, noxx::move(packet));
        break;
    case EtherType::IPv4:
        ipv4::input(*this, src, noxx::move(packet));
        break;
    default:
        break; // drop everything else
    }
}
} // namespace net
