#include <stdio.h>

#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <net/stack.hpp>

#include <noxx/assert.hpp>
#include <noxx/malloc.hpp>

namespace {
auto tx_count = usize(0);

constexpr auto test_mac = net::MacAddr{0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// a NetIf with a settable link state that just counts tx frames
struct CountNetIf : net::NetIf {
    bool up = false;

    auto is_up() const -> bool override {
        return up;
    }
    auto get_mac_addr() const -> net::MacAddrRef override {
        return test_mac;
    }
    auto tx(net::AutoPacket /*packet*/) -> coop::Async<bool> override {
        tx_count += 1;
        co_return true;
    }
};

// drive a test coroutine to completion on a fresh runner; returns its result
auto run_sync(coop::Async<bool> task) -> bool {
    auto runner = coop::Runner();
    auto ok     = false;
    if(!runner.push_task([](coop::Async<bool> task, bool& ok) -> coop::Async<void> {
           ok = co_await noxx::move(task);
       }(noxx::move(task), ok))) {
        return false;
    }
    runner.run();
    return ok;
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

auto test_stack_rx() -> coop::Async<bool> {
    constexpr auto error_value = false;

    net::packet_pool_init();
    auto netif = CountNetIf();
    auto stack = net::Stack();
    stack.init(netif);
    co_ensure(netif.stack == &stack);

    // rx is inline now: co_await netif.rx demuxes each frame and frees it. these
    // carry no valid ethertype, so they are dropped and return to the pool
    const auto before = net::packet_pool_avail();
    for(auto i = usize(0); i < 3; i += 1) {
        auto packet = net::AutoPacket(net::packet_alloc());
        co_ensure(packet.get() != nullptr);
        co_ensure(packet->append(60) != nullptr);
        co_await netif.rx(noxx::move(packet));
    }
    co_ensure(net::packet_pool_avail() == before);

    // tx path: refused while link is down, forwarded when up
    tx_count = 0;
    co_ensure(!co_await stack.send(net::AutoPacket(net::packet_alloc())));
    netif.up = true;
    co_ensure(co_await stack.send(net::AutoPacket(net::packet_alloc())));
    co_ensure(tx_count == 1);
    co_return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;

    static auto heap = noxx::Array<u8, 1 << 20>(); // coop task/frame allocations
    noxx::set_heap(heap.data, heap.size());

    ensure(test_packet_pool());
    ensure(test_packet_room());
    ensure(run_sync(test_stack_rx()));
    printf("all tests passed\n");
    return 0;
}
