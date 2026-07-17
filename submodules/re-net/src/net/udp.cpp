#include <noxx/endian.hpp>

#include "stack.hpp"
#include "udp.hpp"

#include <noxx/assert.hpp>

namespace net::udp {
namespace {
// unfolded ones-complement word sum; an odd trailing byte is zero-padded
auto sum_words(u32 sum, const noxx::Span<const u8> data) -> u32 {
    for(auto i = usize(0); i < data.size(); i += 2) {
        const auto hi = u32(data[i]);
        const auto lo = i + 1 < data.size() ? u32(data[i + 1]) : u32(0);
        sum += (hi << 8) | lo;
    }
    return sum;
}
} // namespace

auto checksum(const IPv4Addr src, const IPv4Addr dst, const noxx::Span<const u8> segment) -> u16 {
    struct Pseudo {
        IPv4Addr src;
        IPv4Addr dst;
        u8       pad;
        u8       proto;
        u16      len;
    } __attribute__((packed));
    static_assert(sizeof(Pseudo) == 12);

    const auto psoude = Pseudo{
        .src   = src,
        .dst   = dst,
        .pad   = 0,
        .proto = ipv4::Proto::Udp,
        .len   = noxx::byteswap(u16(segment.size())),
    };

    auto sum = sum_words(0, {(const u8*)&psoude, sizeof(psoude)});
    sum      = sum_words(sum, segment);
    while((sum >> 16) != 0) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return u16(~sum);
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
