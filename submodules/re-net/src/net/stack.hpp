// per-interface l3 state and the rx dispatch queue
#pragma once
#include "arp.hpp"
#include "ip.hpp"
#include "netif.hpp"

namespace net {
// the embedding drives the stack: the driver enqueues frames via netif.rx(),
// a task loop calls dispatch() until it returns false and tick() once per
// period. on the board that loop is a coop task; on the host it is the
// harness main loop.
struct Stack {
    // private
    // rx queue, linked through Packet::next
    Packet* rx_head = nullptr;
    Packet* rx_tail = nullptr;

    // rx delivery hook installed into NetIf; enqueues for dispatch()
    static auto on_rx(NetIf& netif, AutoPacket packet) -> void;

    // ethertype demux (arp / ipv4), drops the rest
    auto process(AutoPacket packet) -> void;

    // public
    NetIf*     netif   = nullptr;
    IPv4Addr   addr    = {};
    IPv4Addr   netmask = {};
    IPv4Addr   gateway = {};
    arp::Table arp     = {};
    u64        now_ms  = 0; // last tick() time, for protocol timers

    // application hook: an icmp echo reply arrived (payload is after the icmp
    // header). used by ping.
    void* app                                                                                          = nullptr;
    auto (*on_icmp_echo_reply)(Stack& self, IPv4Addr src, u16 id, u16 seq, noxx::Span<const u8> data) -> void = nullptr;

    // attach to an interface, installing the rx delivery hook
    auto init(NetIf& netif) -> void;
    // pop and process one queued rx frame; returns false if the queue was empty
    auto dispatch() -> bool;
    // drive protocol timers; now_ms is a monotonic milliseconds clock
    auto tick(u64 now_ms) -> void;
    // transmit through the attached interface
    auto send(AutoPacket packet) -> bool;
    // prepend an ethernet header and transmit
    auto eth_send(MacAddrRef dst, u16 ethertype, AutoPacket packet) -> bool;
    // true if ip is on our local subnet (addr/netmask must be configured)
    auto on_link(IPv4Addr ip) const -> bool;
};
} // namespace net
