// per-interface l3 state and the rx dispatch queue
#pragma once
#include <coop/promise.hpp>
#include <coop/single-event.hpp>

#include "arp.hpp"
#include "ip.hpp"
#include "netif.hpp"

namespace net {
// coop drives the stack: the driver enqueues frames via netif.rx() (which wakes
// rx_event), run() drains and demuxes them, and timer_task() paces the protocol
// timers. the embedder pushes both onto its shared coop::Runner alongside the
// driver's own tasks; single-threaded, so no locking anywhere.
struct Stack {
    // private
    // rx queue, linked through Packet::next
    Packet*           rx_head = nullptr;
    Packet*           rx_tail = nullptr;
    coop::SingleEvent rx_event; // signalled by on_rx, awaited by run()

    // rx delivery hook installed into NetIf; enqueues and wakes run()
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
    void* app                                                                                                 = nullptr;
    auto (*on_icmp_echo_reply)(Stack& self, IPv4Addr src, u16 id, u16 seq, noxx::Span<const u8> data) -> void = nullptr;

    // attach to an interface, installing the rx delivery hook
    auto init(NetIf& netif) -> void;
    // rx loop: drain the queue then sleep until on_rx wakes rx_event. push onto
    // the embedder's Runner; never returns.
    auto run() -> coop::Async<void>;
    // protocol-timer loop: ticks arp on a fixed period. push onto the Runner.
    auto timer_task() -> coop::Async<void>;
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
