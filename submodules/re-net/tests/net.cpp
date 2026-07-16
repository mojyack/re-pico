// phase n1 protocol tests: checksum, arp reply/resolve, icmp echo, ping flow.
// the datapath is coroutine-based, so each test is an Async driven to completion
// on a throwaway Runner; a capturing NetIf collects every tx frame so the test
// can inspect the wire.
#include <stdio.h>

#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <net/arp.hpp>
#include <net/ethernet.hpp>
#include <net/icmp.hpp>
#include <net/ip.hpp>
#include <net/stack.hpp>

#include <noxx/assert.hpp>
#include <noxx/endian.hpp>
#include <noxx/malloc.hpp>

namespace {
constexpr auto bcast    = net::MacAddr{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr auto our_mac  = net::MacAddr{0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
constexpr auto peer_mac = net::MacAddr{0x02, 0x00, 0x00, 0x00, 0x00, 0x02};
constexpr auto our_ip   = net::IPv4Addr{192, 168, 7, 2};
constexpr auto peer_ip  = net::IPv4Addr{192, 168, 7, 1};

// captured tx frames, fifo
auto cap      = noxx::Array<net::Packet*, 16>();
auto cap_head = usize(0);
auto cap_tail = usize(0);

// a NetIf that captures every tx frame instead of putting it on a wire
struct CapNetIf : net::NetIf {
    auto is_up() const -> bool override {
        return true;
    }
    auto get_mac_addr() const -> net::MacAddrRef override {
        return our_mac;
    }
    auto tx(net::AutoPacket packet) -> coop::Async<bool> override {
        cap[cap_tail++ % 16] = packet.release();
        co_return true;
    }
};

auto cap_pop() -> net::AutoPacket {
    if(cap_head == cap_tail) {
        return net::AutoPacket(nullptr);
    }
    return net::AutoPacket(cap[cap_head++ % 16]);
}

auto cap_count() -> usize {
    return cap_tail - cap_head;
}

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

// deliver a raw ethernet frame and run it through the stack
auto inject(net::NetIf& netif, const noxx::Span<const u8> bytes) -> coop::Async<void> {
    auto packet  = net::AutoPacket(net::packet_alloc());
    packet->head = 0;
    packet->len  = u16(bytes.size());
    noxx::memcpy(packet->buf, bytes.data, bytes.size());
    co_await netif.rx(noxx::move(packet));
}

auto make_stack(CapNetIf& netif, net::Stack& stack) -> void {
    netif         = CapNetIf();
    stack         = net::Stack();
    stack.addr    = our_ip;
    stack.netmask = {255, 255, 255, 0};
    stack.gateway = {192, 168, 7, 254};
    stack.init(netif);
}

auto put_eth(u8* const p, const net::MacAddr dst, const net::MacAddr src, const u16 type) -> void {
    auto& eth = *(net::EthernetHeader*)p;
    noxx::memcpy(eth.dst.data, dst.data, 6);
    noxx::memcpy(eth.src.data, src.data, 6);
    eth.ethertype = noxx::byteswap(type);
}

auto test_checksum() -> bool {
    constexpr auto error_value = false;

    // a header with a valid checksum must sum to zero over its full extent
    auto header        = net::ipv4::Header{};
    header.version_ihl = 0x45;
    header.total_len   = noxx::byteswap(u16(60));
    header.ttl         = 64;
    header.proto       = net::ipv4::Proto::Icmp;
    header.src         = our_ip;
    header.dst         = peer_ip;
    header.checksum    = 0;
    header.checksum    = noxx::byteswap(net::ipv4::checksum({(const u8*)&header, sizeof(header)}));
    ensure(net::ipv4::checksum({(const u8*)&header, sizeof(header)}) == 0);
    return true;
}

auto test_arp_reply() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    cap_head = cap_tail = 0;

    // who-has our_ip, tell peer_ip
    auto frame = noxx::Array<u8, 42>();
    put_eth(frame.data, {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}, peer_mac, net::EtherType::Arp);
    const auto body = net::arp::build_request(peer_mac, peer_ip, our_ip);
    noxx::memcpy(frame.data + 14, &body, sizeof(body));
    co_await inject(netif, frame);

    // exactly one reply frame
    co_ensure(cap_count() == 1);
    auto reply = cap_pop();
    co_ensure(reply->len == 42);
    const auto& eth = *(const net::EthernetHeader*)reply->data();
    co_ensure(noxx::byteswap(eth.ethertype) == net::EtherType::Arp);
    co_ensure(eth.dst == peer_mac);
    co_ensure(eth.src == our_mac);
    const auto& ap = *(const net::arp::Packet*)(reply->data() + 14);
    co_ensure(noxx::byteswap(ap.op) == net::arp::Op::Reply);
    co_ensure(ap.sender_ip == our_ip);
    co_ensure(ap.sender_mac == our_mac);
    co_ensure(ap.target_ip == peer_ip);

    // and the sender got learned
    co_unwrap(mac, net::arp::lookup(stack.arp, peer_ip));
    co_ensure(mac == peer_mac);
    co_return true;
}

// build an ethernet+ipv4+icmp echo frame into out; returns total length
auto build_echo(u8* const out, const net::MacAddr src_mac, const net::IPv4Addr src_ip, const net::IPv4Addr dst_ip, const u8 icmp_type, const u16 id, const u16 seq, const usize payload_len) -> usize {
    put_eth(out, our_mac, src_mac, net::EtherType::IPv4);

    const auto icmp_len = sizeof(net::icmp::EchoHeader) + payload_len;
    auto&      icmp     = *(net::icmp::EchoHeader*)(out + 14 + sizeof(net::ipv4::Header));
    icmp.type           = icmp_type;
    icmp.code           = 0;
    icmp.checksum       = 0;
    icmp.id             = noxx::byteswap(id);
    icmp.seq            = noxx::byteswap(seq);
    for(auto i = usize(0); i < payload_len; i += 1) {
        ((u8*)&icmp)[sizeof(net::icmp::EchoHeader) + i] = u8(0xa0 + i);
    }
    icmp.checksum = noxx::byteswap(net::ipv4::checksum({(const u8*)&icmp, icmp_len}));

    auto& ip       = *(net::ipv4::Header*)(out + 14);
    ip             = net::ipv4::Header{};
    ip.version_ihl = 0x45;
    ip.total_len   = noxx::byteswap(u16(sizeof(net::ipv4::Header) + icmp_len));
    ip.ttl         = 64;
    ip.proto       = net::ipv4::Proto::Icmp;
    ip.src         = src_ip;
    ip.dst         = dst_ip;
    ip.checksum    = noxx::byteswap(net::ipv4::checksum({(const u8*)&ip, sizeof(ip)}));
    return 14 + sizeof(net::ipv4::Header) + icmp_len;
}

auto test_icmp_echo_reply() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    cap_head = cap_tail = 0;

    // learn the peer first so the reply goes out without an arp round trip
    co_await net::arp::cache(stack, peer_ip, peer_mac);

    auto       frame = noxx::Array<u8, 128>();
    const auto n     = build_echo(frame.data, peer_mac, peer_ip, our_ip, net::icmp::Type::EchoRequest, 0x1234, 7, 16);
    co_await inject(netif, {frame.data, n});

    co_ensure(cap_count() == 1);
    auto        reply = cap_pop();
    const auto& eth   = *(const net::EthernetHeader*)reply->data();
    co_ensure(noxx::byteswap(eth.ethertype) == net::EtherType::IPv4);
    co_ensure(eth.dst == peer_mac);
    const auto& ip = *(const net::ipv4::Header*)(reply->data() + 14);
    co_ensure(ip.proto == net::ipv4::Proto::Icmp);
    co_ensure(ip.src == our_ip);
    co_ensure(ip.dst == peer_ip);
    co_ensure(net::ipv4::checksum({(const u8*)&ip, sizeof(ip)}) == 0); // valid ip checksum
    const auto  icmp_len = usize(noxx::byteswap(ip.total_len)) - sizeof(ip);
    const auto& icmp     = *(const net::icmp::EchoHeader*)(reply->data() + 14 + sizeof(ip));
    co_ensure(icmp.type == net::icmp::Type::EchoReply);
    co_ensure(noxx::byteswap(icmp.id) == 0x1234);
    co_ensure(noxx::byteswap(icmp.seq) == 7);
    co_ensure(net::ipv4::checksum({(const u8*)&icmp, icmp_len}) == 0); // valid icmp checksum
    co_return true;
}

auto reply_id  = u16(0);
auto reply_seq = u16(0);
auto reply_len = usize(0);

auto on_echo_reply(net::Stack& /*self*/, const net::IPv4Addr /*src*/, const u16 id, const u16 seq, const noxx::Span<const u8> data) -> void {
    reply_id  = id;
    reply_seq = seq;
    reply_len = data.size();
}

auto test_ping_flow() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    stack.on_icmp_echo_reply = on_echo_reply;
    cap_head = cap_tail = 0;

    // outbound ping to an unresolved on-link host: only an arp request goes out
    co_ensure(co_await net::icmp::send_echo(stack, peer_ip, 0x2222, 1, 32));
    co_ensure(cap_count() == 1);
    auto        req = cap_pop();
    const auto& eth = *(const net::EthernetHeader*)req->data();
    co_ensure(noxx::byteswap(eth.ethertype) == net::EtherType::Arp);
    co_ensure(eth.dst == bcast);

    // arp reply flushes the pending echo request
    auto af = noxx::Array<u8, 42>();
    put_eth(af.data, our_mac, peer_mac, net::EtherType::Arp);
    auto ab = net::arp::build_request(peer_mac, peer_ip, our_ip);
    ab.op   = noxx::byteswap(u16(net::arp::Op::Reply));
    noxx::memcpy(ab.target_mac.data, our_mac.data, 6);
    ab.target_ip = our_ip;
    noxx::memcpy(af.data + 14, &ab, sizeof(ab));
    co_await inject(netif, af);

    co_ensure(cap_count() == 1);
    auto        echo = cap_pop();
    const auto& eeth = *(const net::EthernetHeader*)echo->data();
    co_ensure(noxx::byteswap(eeth.ethertype) == net::EtherType::IPv4);
    co_ensure(eeth.dst == peer_mac);
    const auto& ip = *(const net::ipv4::Header*)(echo->data() + 14);
    co_ensure(ip.proto == net::ipv4::Proto::Icmp);
    co_ensure(ip.dst == peer_ip);

    // feed the matching echo reply back; the callback must fire
    reply_id = reply_seq = 0;
    reply_len            = 0;
    auto       rf        = noxx::Array<u8, 128>();
    const auto n         = build_echo(rf.data, peer_mac, peer_ip, our_ip, net::icmp::Type::EchoReply, 0x2222, 1, 32);
    co_await inject(netif, {rf.data, n});
    co_ensure(reply_id == 0x2222);
    co_ensure(reply_seq == 1);
    co_ensure(reply_len == 32);
    co_return true;
}

auto test_arp_retry() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    cap_head = cap_tail = 0;

    // unresolved send queues one request
    co_ensure(co_await net::icmp::send_echo(stack, peer_ip, 1, 1, 0));
    co_ensure(cap_count() == 1);
    (void)cap_pop();

    // ticking past each retry interval re-requests, up to max_retries total
    auto now = u64(0);
    for(auto i = usize(0); i < net::arp::max_retries; i += 1) {
        now += net::arp::retry_interval_ms;
        co_await stack.tick(now);
    }
    // (max_retries - 1) additional requests after the first
    co_ensure(cap_count() == net::arp::max_retries - 1);
    while(cap_pop().get() != nullptr) {
    }

    // one more tick gives up and drops the entry
    now += net::arp::retry_interval_ms;
    co_await stack.tick(now);
    co_ensure(!net::arp::lookup(stack.arp, peer_ip));
    co_return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;

    static auto heap = noxx::Array<u8, 1 << 20>(); // coop task/frame allocations
    noxx::set_heap(heap.data, heap.size());
    net::packet_pool_init();

    ensure(test_checksum());
    ensure(run_sync(test_arp_reply()));
    ensure(run_sync(test_icmp_echo_reply()));
    ensure(run_sync(test_ping_flow()));
    ensure(run_sync(test_arp_retry()));
    printf("all tests passed\n");
    return 0;
}
