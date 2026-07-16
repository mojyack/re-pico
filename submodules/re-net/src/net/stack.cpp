#include <noxx/endian.hpp>

#include "ethernet.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net {
auto Stack::on_rx(NetIf& netif, AutoPacket packet) -> void {
    // enqueue only; the frame is processed later on the dispatch loop
    auto&      self = *(Stack*)netif.stack;
    const auto raw  = packet.release();
    raw->next       = nullptr;
    if(self.rx_tail == nullptr) {
        self.rx_head = raw;
    } else {
        self.rx_tail->next = raw;
    }
    self.rx_tail = raw;
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

auto Stack::tick(const u64 /*now_ms*/) -> void {
    // no protocol timers yet (arp aging arrives with n1)
}

auto Stack::send(AutoPacket packet) -> bool {
    constexpr auto error_value = false;

    ensure(netif != nullptr && netif->tx != nullptr && netif->link_up);
    return netif->tx(*netif, noxx::move(packet));
}

auto Stack::process(AutoPacket packet) -> void {
    if(packet->len < sizeof(EthernetHeader)) {
        return;
    }
    const auto& header = *(const EthernetHeader*)packet->data();
    switch(noxx::byteswap(header.ethertype)) {
    case EtherType::Arp:
    case EtherType::IPv4:
    default:
        // n1 demuxes these; n0 drops everything
        break;
    }
}
} // namespace net
