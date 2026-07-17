// user datagram protocol (rfc 768)
#pragma once
#include <coop/promise.hpp>
#include <net/ip.hpp>
#include <net/packet.hpp>
#include <noxx/array.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

namespace net {
struct Stack;
} // namespace net

namespace net::udp {
// udp header; multi-byte scalars are big-endian
struct Header {
    u16 src_port;
    u16 dst_port;
    u16 len; // header + payload
    u16 checksum;
} __attribute__((packed));
static_assert(sizeof(Header) == 8);

struct Socket {
    u16   port = 0;
    void* ctx  = nullptr;

    auto (*on_rx)(Stack& stack, Socket& self, IPv4Addr src, u16 src_port, noxx::Span<const u8> data) -> void = nullptr;
};

struct Table {
    noxx::Array<Socket*, 4> sockets = {}; // max 4 sockets
};

// ones-complement checksum over the ipv4 pseudo-header and the udp segment
// (host order return); a valid received segment sums to zero
auto checksum(IPv4Addr src, IPv4Addr dst, noxx::Span<const u8> segment) -> u16;

// register/unregister a socket; bind fails on a duplicate port or a full table
auto bind(Stack& stack, Socket& socket) -> bool;
auto unbind(Stack& stack, Socket& socket) -> void;

// handle a received udp datagram (ip header already consumed); dst is the ip
// destination, needed for the checksum pseudo-header
auto input(Stack& stack, IPv4Addr src, IPv4Addr dst, AutoPacket packet) -> coop::Async<void>;

// prepend a udp header and send toward dst; the packet's data() must point at
// the payload with headroom for the lower headers
auto send(Stack& stack, IPv4Addr dst, u16 src_port, u16 dst_port, AutoPacket packet) -> coop::Async<bool>;
} // namespace net::udp
