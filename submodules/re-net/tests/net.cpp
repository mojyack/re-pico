// phase n1/n2 protocol tests: checksum, arp reply/resolve, icmp echo, ping
// flow, udp rx/tx, dhcp lease acquisition. the datapath is coroutine-based, so
// each test is an Async driven to completion on a throwaway Runner; a capturing
// NetIf collects every tx frame so the test can inspect the wire.
#include <stdio.h>

#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <net/arp.hpp>
#include <net/dhcp.hpp>
#include <net/ethernet.hpp>
#include <net/icmp.hpp>
#include <net/ip.hpp>
#include <net/stack.hpp>
#include <net/udp.hpp>

#include <noxx/assert.hpp>
#include <noxx/buf-reader.hpp>
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
// build an ethernet+ipv4+udp frame into out; returns total length. a zero udp
// checksum marks "not computed" on the wire, so compute the real one
auto build_udp(u8* const out, const net::MacAddr src_mac, const net::IPv4Addr src_ip, const net::IPv4Addr dst_ip, const u16 src_port, const u16 dst_port, const noxx::Span<const u8> payload) -> usize {
    put_eth(out, our_mac, src_mac, net::EtherType::IPv4);

    const auto udp_len = sizeof(net::udp::Header) + payload.size();
    auto&      udp     = *(net::udp::Header*)(out + 14 + sizeof(net::ipv4::Header));
    udp.src_port       = noxx::byteswap(src_port);
    udp.dst_port       = noxx::byteswap(dst_port);
    udp.len            = noxx::byteswap(u16(udp_len));
    udp.checksum       = 0;
    noxx::memcpy((u8*)&udp + sizeof(udp), payload.data, payload.size());
    udp.checksum = noxx::byteswap(net::udp::checksum(src_ip, dst_ip, {(const u8*)&udp, udp_len}));

    auto& ip       = *(net::ipv4::Header*)(out + 14);
    ip             = net::ipv4::Header{};
    ip.version_ihl = 0x45;
    ip.total_len   = noxx::byteswap(u16(sizeof(net::ipv4::Header) + udp_len));
    ip.ttl         = 64;
    ip.proto       = net::ipv4::Proto::Udp;
    ip.src         = src_ip;
    ip.dst         = dst_ip;
    ip.checksum    = noxx::byteswap(net::ipv4::checksum({(const u8*)&ip, sizeof(ip)}));
    return 14 + sizeof(net::ipv4::Header) + udp_len;
}

auto udp_rx_src       = net::IPv4Addr();
auto udp_rx_src_port  = u16(0);
auto udp_rx_data      = noxx::Array<u8, 64>();
auto udp_rx_len       = usize(0);

auto on_udp_rx(net::Stack& /*stack*/, net::udp::Socket& /*self*/, const net::IPv4Addr src, const u16 src_port, const noxx::Span<const u8> data) -> void {
    udp_rx_src      = src;
    udp_rx_src_port = src_port;
    udp_rx_len      = data.size();
    noxx::memcpy(udp_rx_data.data, data.data, data.size());
}

auto test_udp() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    cap_head = cap_tail = 0;

    auto socket   = net::udp::Socket();
    socket.port   = 7;
    socket.on_rx  = on_udp_rx;
    co_ensure(net::udp::bind(stack, socket));
    auto dup = net::udp::Socket();
    dup.port = 7;
    co_ensure(!net::udp::bind(stack, dup), "duplicate bind must fail");

    // inbound datagram reaches the socket hook
    constexpr auto payload = noxx::to_array<u8>({'h', 'e', 'l', 'l', 'o'});
    auto           frame   = noxx::Array<u8, 128>();
    const auto     n       = build_udp(frame.data, peer_mac, peer_ip, our_ip, 12345, 7, payload);
    udp_rx_len             = 0;
    co_await inject(netif, {frame.data, n});
    co_ensure(udp_rx_src == peer_ip);
    co_ensure(udp_rx_src_port == 12345);
    co_ensure(udp_rx_len == payload.size());
    co_ensure(noxx::memcmp(udp_rx_data.data, payload.data, payload.size()) == 0);

    // a corrupt udp checksum is dropped (the ip header checksum is unaffected)
    frame[14 + sizeof(net::ipv4::Header) + 6] ^= 0xff;
    udp_rx_len = 0;
    co_await inject(netif, {frame.data, n});
    co_ensure(udp_rx_len == 0, "corrupt datagram must not be delivered");

    // outbound send emits a well-formed datagram
    co_await net::arp::cache(stack, peer_ip, peer_mac);
    const auto headroom = sizeof(net::EthernetHeader) + sizeof(net::ipv4::Header) + sizeof(net::udp::Header);
    auto       out      = net::AutoPacket(net::packet_alloc(headroom));
    co_ensure(out.get() != nullptr);
    const auto body = out->append(payload.size());
    co_ensure(body != nullptr);
    noxx::memcpy(body, payload.data, payload.size());
    co_ensure(co_await net::udp::send(stack, peer_ip, 7, 12345, noxx::move(out)));
    co_ensure(cap_count() == 1);
    auto        sent = cap_pop();
    const auto& ip   = *(const net::ipv4::Header*)(sent->data() + 14);
    co_ensure(ip.proto == net::ipv4::Proto::Udp);
    co_ensure(ip.dst == peer_ip);
    const auto& udp = *(const net::udp::Header*)(sent->data() + 14 + sizeof(ip));
    co_ensure(noxx::byteswap(udp.src_port) == 7);
    co_ensure(noxx::byteswap(udp.dst_port) == 12345);
    const auto udp_len = usize(noxx::byteswap(udp.len));
    co_ensure(udp_len == sizeof(udp) + payload.size());
    co_ensure(net::udp::checksum(ip.src, ip.dst, {(const u8*)&udp, udp_len}) == 0); // valid udp checksum
    co_ensure(noxx::memcmp((const u8*)&udp + sizeof(udp), payload.data, payload.size()) == 0);

    net::udp::unbind(stack, socket);
    co_return true;
}

// dhcp wire helpers

constexpr auto server_ip  = peer_ip;
constexpr auto offered_ip = net::IPv4Addr{192, 168, 7, 50};

// find a dhcp option body in an options block
auto find_option(const noxx::Span<const u8> options, const u8 code) -> noxx::Optional<noxx::Span<const u8>> {
    constexpr auto error_value = noxx::nullopt;

    auto reader = noxx::SpanReader(options);
    while(true) {
        unwrap(c, reader.read(1));
        if(c == net::dhcp::Option::Pad) {
            continue;
        }
        if(c == net::dhcp::Option::End) {
            break;
        }
        unwrap(len, reader.read(1));
        unwrap(body, reader.read_span(len));
        if(c == code) {
            return noxx::Span<const u8>(body);
        }
    }
    return noxx::nullopt;
}

// build an ethernet+ipv4+udp+dhcp server reply into out; returns total length
auto build_dhcp_reply(u8* const out, const u8 msg_type, const u32 xid, const net::IPv4Addr ip_dst) -> usize {
    auto  dhcp   = noxx::Array<u8, sizeof(net::dhcp::Header) + 32>();
    auto& header = *(net::dhcp::Header*)dhcp.data;
    header       = net::dhcp::Header{};
    header.op     = net::dhcp::Op::BootReply;
    header.htype  = net::dhcp::HType::Ethernet;
    header.hlen   = net::MacAddr::size();
    header.xid    = noxx::byteswap(xid);
    header.yiaddr = offered_ip;
    header.cookie = noxx::byteswap(net::dhcp::magic_cookie);
    noxx::memcpy(header.chaddr, our_mac.data, net::MacAddr::size());
    auto p = dhcp.data + sizeof(header);
    *p++   = net::dhcp::Option::MessageType;
    *p++   = 1;
    *p++   = msg_type;
    *p++   = net::dhcp::Option::ServerId;
    *p++   = 4;
    noxx::memcpy(p, server_ip.data, 4), p += 4;
    *p++ = net::dhcp::Option::SubnetMask;
    *p++ = 4;
    *p++ = 255, *p++ = 255, *p++ = 255, *p++ = 0;
    *p++ = net::dhcp::Option::Router;
    *p++ = 4;
    noxx::memcpy(p, server_ip.data, 4), p += 4;
    *p++ = net::dhcp::Option::LeaseTime;
    *p++ = 4;
    *p++ = 0, *p++ = 0, *p++ = 0x0e, *p++ = 0x10; // 3600 s
    *p++ = net::dhcp::Option::End;
    return build_udp(out, peer_mac, server_ip, ip_dst, net::dhcp::server_port, net::dhcp::client_port, {dhcp.data, usize(p - dhcp.data)});
}

// pop one captured frame and validate the dhcp client message layers; returns
// the dhcp payload span within the passed packet
auto check_dhcp_tx(net::AutoPacket& packet, const u8 want_type) -> noxx::Optional<noxx::Span<const u8>> {
    constexpr auto error_value = noxx::nullopt;

    packet = cap_pop();
    ensure(packet.get() != nullptr);
    const auto& eth = *(const net::EthernetHeader*)packet->data();
    ensure(eth.dst == bcast);
    ensure(noxx::byteswap(eth.ethertype) == net::EtherType::IPv4);
    const auto& ip = *(const net::ipv4::Header*)(packet->data() + 14);
    ensure(ip.proto == net::ipv4::Proto::Udp);
    ensure(ip.src == net::ipv4_any);
    ensure(ip.dst == net::ipv4_broadcast);
    const auto& udp = *(const net::udp::Header*)(packet->data() + 14 + sizeof(ip));
    ensure(noxx::byteswap(udp.src_port) == net::dhcp::client_port);
    ensure(noxx::byteswap(udp.dst_port) == net::dhcp::server_port);
    const auto udp_len = usize(noxx::byteswap(udp.len));
    ensure(net::udp::checksum(ip.src, ip.dst, {(const u8*)&udp, udp_len}) == 0);
    const auto& dhcp = *(const net::dhcp::Header*)((const u8*)&udp + sizeof(udp));
    ensure(dhcp.op == net::dhcp::Op::BootRequest);
    ensure(noxx::memcmp(dhcp.chaddr, our_mac.data, 6) == 0);
    ensure(noxx::byteswap(dhcp.cookie) == net::dhcp::magic_cookie);
    auto options = noxx::Span<const u8>{(const u8*)&dhcp + sizeof(dhcp), udp_len - sizeof(udp) - sizeof(dhcp)};
    unwrap(type, find_option(options, net::dhcp::Option::MessageType));
    ensure(type.size() == 1 && type[0] == want_type);
    return noxx::move(options);
}

auto test_dhcp() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto netif = CapNetIf();
    auto stack = net::Stack();
    make_stack(netif, stack);
    stack.addr = stack.netmask = stack.gateway = {}; // unconfigured, dhcp fills these
    cap_head = cap_tail = 0;

    auto  client = net::dhcp::Client();
    auto  handle = coop::TaskHandle();
    auto& runner = *co_await coop::reveal_runner();
    co_ensure(runner.push_task(client.task(stack), &handle));
    co_await coop::sleep_ms(1); // let the discover go out

    // discover on the wire
    co_ensure(cap_count() == 1);
    auto packet = net::AutoPacket();
    co_ensure(check_dhcp_tx(packet, net::dhcp::MessageType::Discover));
    const auto& dhcp = *(const net::dhcp::Header*)(packet->data() + 14 + sizeof(net::ipv4::Header) + sizeof(net::udp::Header));
    const auto  xid  = noxx::byteswap(dhcp.xid);

    // offer (broadcast) elicits a request carrying the offered ip + server id
    auto       frame = noxx::Array<u8, 512>();
    const auto on    = build_dhcp_reply(frame.data, net::dhcp::MessageType::Offer, xid, net::ipv4_broadcast);
    co_await inject(netif, {frame.data, on});
    co_await coop::sleep_ms(1);
    co_ensure(cap_count() == 1);
    co_unwrap(options, check_dhcp_tx(packet, net::dhcp::MessageType::Request));
    co_unwrap(req_ip, find_option(options, net::dhcp::Option::RequestedIp));
    auto addr_span = noxx::Span<const u8>(offered_ip);
    co_ensure(req_ip == addr_span);
    co_unwrap(srv_id, find_option(options, net::dhcp::Option::ServerId));
    addr_span = noxx::Span<const u8>(server_ip);
    co_ensure(srv_id == addr_span);

    // ack (unicast to the offered address, accepted while unconfigured) binds
    const auto an = build_dhcp_reply(frame.data, net::dhcp::MessageType::Ack, xid, offered_ip);
    co_await inject(netif, {frame.data, an});
    co_await coop::sleep_ms(1);
    co_ensure(client.state == net::dhcp::State::Bound);
    co_ensure(stack.addr == offered_ip);
    const auto want_mask = net::IPv4Addr{255, 255, 255, 0};
    co_ensure(stack.netmask == want_mask);
    co_ensure(stack.gateway == server_ip);
    co_ensure(client.lease.lease_s == 3600);

    handle.cancel();
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
    ensure(run_sync(test_udp()));
    ensure(run_sync(test_dhcp()));
    printf("all tests passed\n");
    return 0;
}
