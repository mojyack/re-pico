#pragma once
#include <coop/promise.hpp>
#include <net/ip.hpp>
#include <net/packet.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

// internet control message protocol (rfc 792); echo only
namespace net {
struct Stack;
} // namespace net

namespace net::icmp {
struct Type {
    enum : u8 {
        EchoReply   = 0,
        EchoRequest = 8,
    };
};

// echo request/reply header; id/seq are big-endian
struct EchoHeader {
    u8  type;
    u8  code;
    u16 checksum; // over header + payload
    u16 id;
    u16 seq;
} __attribute__((packed));

static_assert(sizeof(EchoHeader) == 8);

// handle a received icmp message (ip header already consumed); replies to echo
// requests and reports echo replies via Stack::on_icmp_echo_reply
auto input(Stack& stack, IPv4Addr src, AutoPacket packet) -> coop::Async<void>;

// send an echo request of payload_len data bytes toward dst
auto send_echo(Stack& stack, IPv4Addr dst, u16 id, u16 seq, usize payload_len) -> coop::Async<bool>;
} // namespace net::icmp
