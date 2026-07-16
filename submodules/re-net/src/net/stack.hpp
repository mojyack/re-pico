// per-interface l3 state and the coroutine datapath
#pragma once
#include <coop/promise.hpp>

#include "arp.hpp"
#include "ip.hpp"
#include "netif.hpp"

namespace net {
struct Stack {
    // public
    NetIf*     netif = nullptr;
    IPv4Addr   addr;
    IPv4Addr   netmask;
    IPv4Addr   gateway;
    arp::Table arp;
    u64        now_ms = 0; // last tick() time, for protocol timers

    // application hook: an icmp echo reply arrived (payload is after the icmp
    // header). used by ping.
    void* app                                                                                                 = nullptr;
    auto (*on_icmp_echo_reply)(Stack& self, IPv4Addr src, u16 id, u16 seq, noxx::Span<const u8> data) -> void = nullptr;

    // attach to an interface, installing the rx delivery hook
    auto init(NetIf& netif) -> void;
    // protocol-timer loop: ticks arp on a fixed period. push onto the Runner.
    auto timer_task() -> coop::Async<void>;
    // drive protocol timers; now_ms is a monotonic milliseconds clock
    auto tick(u64 now_ms) -> coop::Async<void>;
    // transmit through the attached interface
    auto send(AutoPacket packet) -> coop::Async<bool>;
    // prepend an ethernet header and transmit
    auto eth_send(MacAddrRef dst, u16 ethertype, AutoPacket packet) -> coop::Async<bool>;
    // true if ip is on our local subnet (addr/netmask must be configured)
    auto on_link(IPv4Addr ip) const -> bool;
};
} // namespace net
