#include <noxx/endian.hpp>

#include "stack.hpp"
#include "udp.hpp"

#include <noxx/assert.hpp>

namespace net::udp {
auto checksum(const IPv4Addr src, const IPv4Addr dst, const noxx::Span<const u8> segment) -> u16 {
    return ipv4::l4_checksum(src, dst, ipv4::Proto::Udp, segment);
}

auto bind(Stack& stack, Socket& socket) -> bool {
    constexpr auto error_value = false;

    auto free = (Socket**)nullptr;
    for(auto& slot : stack.udp.sockets.data) {
        if(slot == nullptr) {
            free = free != nullptr ? free : &slot;
            continue;
        }
        ensure(slot->port != socket.port, "port in use");
    }
    ensure(free != nullptr, "socket table full");
    *free = &socket;
    return true;
}

auto unbind(Stack& stack, Socket& socket) -> void {
    for(auto& slot : stack.udp.sockets.data) {
        if(slot == &socket) {
            slot = nullptr;
        }
    }
}

auto input(Stack& stack, const IPv4Addr src, const IPv4Addr dst, AutoPacket packet) -> coop::Async<void> {
    if(packet->len < sizeof(Header)) {
        co_return;
    }
    const auto& header = *(const Header*)packet->data();
    const auto  len    = usize(noxx::byteswap(header.len));
    if(len < sizeof(Header) || len > packet->len) {
        co_return;
    }
    packet->len = u16(len);
    // a checksum of zero means the sender did not compute one
    if(header.checksum != 0 && checksum(src, dst, {packet->data(), len}) != 0) {
        co_return; // corrupt
    }
    const auto src_port = noxx::byteswap(header.src_port);
    const auto dst_port = noxx::byteswap(header.dst_port);
    for(auto& slot : stack.udp.sockets.data) {
        if(slot != nullptr && slot->port == dst_port && slot->on_rx != nullptr) {
            slot->on_rx(stack, *slot, src, src_port, {packet->data() + sizeof(Header), len - sizeof(Header)});
            co_return;
        }
    }
    // no listener; icmp port-unreachable is a later phase
}

auto send(Stack& stack, const IPv4Addr dst, const u16 src_port, const u16 dst_port, AutoPacket packet) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto len = sizeof(Header) + packet->len;
    co_ensure(len <= 0xffff);
    const auto raw = packet->prepend(sizeof(Header));
    co_ensure(raw != nullptr, "no headroom for udp header");

    auto& header    = *(Header*)raw;
    header.src_port = noxx::byteswap(src_port);
    header.dst_port = noxx::byteswap(dst_port);
    header.len      = noxx::byteswap(u16(len));
    header.checksum = 0;
    // the source in the pseudo-header must match what ipv4::output will emit
    const auto sum  = checksum(stack.addr, dst, {raw, len});
    header.checksum = noxx::byteswap(sum != 0 ? sum : u16(0xffff));

    co_return co_await ipv4::output(stack, dst, ipv4::Proto::Udp, noxx::move(packet));
}
} // namespace net::udp
