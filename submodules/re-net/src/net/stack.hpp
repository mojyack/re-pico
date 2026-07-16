// per-interface l3 state and the rx dispatch queue
#pragma once
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

    // ethertype demux; n1 fills in arp/ipv4, n0 drops everything
    auto process(AutoPacket packet) -> void;

    // public
    NetIf*   netif   = nullptr;
    IPv4Addr addr    = {};
    IPv4Addr netmask = {};
    IPv4Addr gateway = {};

    // attach to an interface, installing the rx delivery hook
    auto init(NetIf& netif) -> void;
    // pop and process one queued rx frame; returns false if the queue was empty
    auto dispatch() -> bool;
    // drive protocol timers; now_ms is a monotonic milliseconds clock
    auto tick(u64 now_ms) -> void;
    // transmit through the attached interface
    auto send(AutoPacket packet) -> bool;
};
} // namespace net
