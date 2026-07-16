#include <stdio.h>

#include <net/stack.hpp>

#include <noxx/assert.hpp>

namespace {
auto tx_count = usize(0);

auto test_tx(net::NetIf& /*netif*/, net::AutoPacket /*packet*/) -> bool {
    tx_count += 1;
    return true;
}

auto test_packet_pool() -> bool {
    constexpr auto error_value = false;

    net::packet_pool_init();
    const auto total = net::packet_pool_avail();
    ensure(total > 0);

    // exhaust the pool, then free everything back
    auto packets = noxx::Array<net::Packet*, 64>();
    ensure(total <= packets.size());
    for(auto i = usize(0); i < total; i += 1) {
        packets[i] = net::packet_alloc();
        ensure(packets[i] != nullptr);
    }
    ensure(net::packet_pool_avail() == 0);
    ensure(net::packet_alloc() == nullptr);
    for(auto i = usize(0); i < total; i += 1) {
        net::packet_free(packets[i]);
    }
    ensure(net::packet_pool_avail() == total);
    return true;
}

auto test_packet_room() -> bool {
    constexpr auto error_value = false;

    net::packet_pool_init();
    auto packet = net::AutoPacket(net::packet_alloc(64));
    ensure(packet.get() != nullptr);
    ensure(packet->headroom() == 64);
    ensure(packet->len == 0);

    // append a payload, prepend a header, then consume it back off
    ensure(packet->append(100) != nullptr);
    ensure(packet->len == 100);
    ensure(packet->prepend(14) != nullptr);
    ensure(packet->len == 114);
    ensure(packet->headroom() == 50);
    ensure(packet->consume(14));
    ensure(packet->len == 100);

    // bounds
    ensure(packet->prepend(1000) == nullptr);
    ensure(packet->append(net::packet_capacity) == nullptr);
    ensure(!packet->consume(1000));
    return true;
}

auto test_stack_rx() -> bool {
    constexpr auto error_value = false;

    net::packet_pool_init();
    auto netif = net::NetIf{.tx = test_tx};
    auto stack = net::Stack();
    stack.init(netif);
    ensure(netif.rx != nullptr);

    // nothing queued yet
    ensure(!stack.dispatch());

    // deliver three frames, dispatch must pop exactly three
    for(auto i = usize(0); i < 3; i += 1) {
        auto packet = net::AutoPacket(net::packet_alloc());
        ensure(packet.get() != nullptr);
        ensure(packet->append(60) != nullptr);
        netif.rx(netif, noxx::move(packet));
    }
    ensure(stack.dispatch());
    ensure(stack.dispatch());
    ensure(stack.dispatch());
    ensure(!stack.dispatch());
    // dispatched packets must return to the pool
    ensure(net::packet_pool_avail() > 0);

    // tx path: refused while link is down, forwarded when up
    tx_count = 0;
    ensure(!stack.send(net::AutoPacket(net::packet_alloc())));
    netif.link_up = true;
    ensure(stack.send(net::AutoPacket(net::packet_alloc())));
    ensure(tx_count == 1);
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;

    ensure(test_packet_pool());
    ensure(test_packet_room());
    ensure(test_stack_rx());
    printf("all tests passed\n");
    return 0;
}
