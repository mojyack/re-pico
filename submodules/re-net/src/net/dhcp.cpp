#include <coop/platform.hpp>
#include <coop/single-event.hpp>
#include <coop/timer.hpp>
#include <noxx/endian.hpp>

#include "dhcp.hpp"
#include "ethernet.hpp"
#include "packet-buf.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net::dhcp {
namespace {
// build and send one client message; broadcast, except a renewal which goes
// unicast to the leasing server
auto send_message(Client& client, Stack& stack, const u8 type) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto renewing = client.state == State::Renewing;
    const auto headroom = sizeof(EthernetHeader) + sizeof(ipv4::Header) + sizeof(udp::Header);
    auto       packet   = AutoPacket(packet_alloc(headroom));
    co_ensure(packet.get() != nullptr);

    auto header = Header{
        .op     = Op::BootRequest,
        .htype  = HType::Ethernet,
        .hlen   = MacAddr::size(),
        .xid    = noxx::byteswap(client.xid),
        .cookie = noxx::byteswap(magic_cookie),
    };
    if(renewing) {
        header.ciaddr = stack.addr;
    }
    noxx::memcpy(header.chaddr, stack.netif->get_mac_addr().data, MacAddr::size());

    auto writer = PacketWriter(*packet);
    co_ensure(writer.append_obj(header));

    const auto opt = [&writer](const u8 code, const noxx::Span<const u8> body) -> bool {
        const auto mem = writer.alloc(2 + body.size());
        ensure(mem != nullptr);
        mem[0] = code;
        mem[1] = u8(body.size());
        noxx::memcpy(mem + 2, body.data, body.size());
        return true;
    };
    co_ensure(opt(Option::MessageType, {&type, 1}));
    if(type == MessageType::Request && !renewing) {
        co_ensure(opt(Option::RequestedIp, client.lease.addr));
        co_ensure(opt(Option::ServerId, client.lease.server_id));
    }
    constexpr auto params = noxx::to_array<u8>({Option::SubnetMask, Option::Router, Option::Dns});
    co_ensure(opt(Option::ParamReqList, params));
    constexpr auto end = u8(Option::End);
    co_ensure(writer.append_obj(end));

    const auto dst = renewing ? client.lease.server_id : ipv4_broadcast;
    co_return co_await udp::send(stack, dst, client_port, server_port, noxx::move(packet));
}

// extract the message type and lease parameters from a boot-reply's options
auto parse_reply(const Header& header, const noxx::Span<const u8> options, u8& type, Lease& lease) -> bool {
    constexpr auto error_value = false;

    type        = 0;
    lease       = Lease();
    lease.addr  = header.yiaddr;
    auto reader = noxx::SpanReader(options);
    while(true) {
        unwrap(code, reader.read(1));
        if(code == Option::Pad) {
            continue;
        }
        if(code == Option::End) {
            break;
        }
        unwrap(len, reader.read(1));
        unwrap(body, reader.read_span(len));
        switch(code) {
        case Option::MessageType:
            ensure(len == 1);
            type = body[0];
            break;
        case Option::SubnetMask:
            ensure(len == IPv4Addr::size());
            noxx::memcpy(lease.netmask.data, body.data, IPv4Addr::size());
            break;
        case Option::Router:
            ensure(len >= IPv4Addr::size()); // may list several; take the first
            noxx::memcpy(lease.router.data, body.data, IPv4Addr::size());
            break;
        case Option::ServerId:
            ensure(len == IPv4Addr::size());
            noxx::memcpy(lease.server_id.data, body.data, IPv4Addr::size());
            break;
        case Option::LeaseTime:
            ensure(len == 4);
            lease.lease_s = u32(body[0]) << 24 | u32(body[1]) << 16 | u32(body[2]) << 8 | u32(body[3]);
            break;
        default:
            break;
        }
    }
    ensure(type != 0, "no message type option");
    return true;
}

auto on_socket_rx(Stack& stack, udp::Socket& socket, const IPv4Addr /*src*/, const u16 /*src_port*/, const noxx::Span<const u8> data) -> void {
    auto& client = *(Client*)socket.ctx;
    if(data.size() < sizeof(Header)) {
        return;
    }
    const auto& header = *(const Header*)data.data;
    if(header.op != Op::BootReply || noxx::byteswap(header.xid) != client.xid || noxx::byteswap(header.cookie) != magic_cookie) {
        return;
    }
    if(noxx::memcmp(header.chaddr, stack.netif->get_mac_addr().data, MacAddr::size()) != 0) {
        return; // for someone else
    }
    auto type  = u8(0);
    auto lease = Lease();
    if(!parse_reply(header, {data.data + sizeof(Header), data.size() - sizeof(Header)}, type, lease)) {
        return;
    }
    // only messages that advance the current state wake the task
    switch(client.state) {
    case State::Selecting:
        if(type != MessageType::Offer || lease.server_id == IPv4Addr{}) {
            return;
        }
        break;
    case State::Requesting:
    case State::Renewing:
        if(type != MessageType::Ack && type != MessageType::Nak) {
            return;
        }
        break;
    default:
        return;
    }
    client.reply_type  = type;
    client.reply_lease = lease;
    client.reply.notify();
}

// send one message per attempt until a state-advancing reply arrives
auto transact(Client& client, Stack& stack, const u8 type) -> coop::Async<bool> {
    constexpr auto error_value = false;

    for(auto i = u32(0); i < max_tries; i += 1) {
        client.reply_type = 0;
        client.reply      = coop::SingleEvent(); // drop any stale notification
        co_ensure(co_await send_message(client, stack, type));
        if(co_await coop::wait_for_event(client.reply, reply_timeout_ms * 1000)) {
            co_return true;
        }
    }
    co_return false; // no reply
}
} // namespace

auto Client::task(Stack& stack) -> coop::Async<void> {
    socket.port  = client_port;
    socket.ctx   = this;
    socket.on_rx = on_socket_rx;
    if(!udp::bind(stack, socket)) {
        co_return;
    }
    while(true) {
        // obtain a lease: discover/offer then request/ack, all broadcast
        stack.addr    = {};
        stack.netmask = {};
        stack.gateway = {};
        xid           = u32(coop::now_us());
        state         = State::Selecting;
        if(!co_await transact(*this, stack, MessageType::Discover)) {
            continue;
        }
        lease = reply_lease;
        state = State::Requesting;
        if(!co_await transact(*this, stack, MessageType::Request) || reply_type != MessageType::Ack) {
            continue; // timeout or nak: start over
        }
        lease         = reply_lease;
        stack.addr    = lease.addr;
        stack.netmask = lease.netmask;
        stack.gateway = lease.router;
        state         = State::Bound;

        // renew at t1 (half the lease) until an attempt fails, then rebind
        while(true) {
            const auto lease_s = lease.lease_s != 0 ? lease.lease_s : default_lease_s;
            co_await coop::sleep_ms(u64(lease_s) * 1000 / 2);
            state = State::Renewing;
            xid   = u32(coop::now_us());
            if(!co_await transact(*this, stack, MessageType::Request) || reply_type != MessageType::Ack) {
                break;
            }
            lease.lease_s = reply_lease.lease_s;
            state         = State::Bound;
        }
    }
}
} // namespace net::dhcp
