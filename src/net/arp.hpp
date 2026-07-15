#pragma once
#include <net/ip.hpp>
#include <net/mac-addr.hpp>
#include <noxx/int.hpp>
#include <noxx/span.hpp>

// address resolution protocol over ethernet/ipv4 (rfc 826)
namespace net::arp {
constexpr auto ethertype   = u16(0x0806);
constexpr auto packet_size = usize(28); // fixed for the ethernet/ipv4 pair

struct Op {
    enum : u16 {
        Request = 1,
        Reply   = 2,
    };
};

// arp packet for the ethernet/ipv4 pair; multi-byte scalars are big-endian
struct Packet {
    u16      hw_type;
    u16      proto_type;
    u8       hw_len;
    u8       proto_len;
    u16      op;
    MacAddr  sender_mac;
    IPv4Addr sender_ip;
    MacAddr  target_mac;
    IPv4Addr target_ip;
} __attribute__((packed));

static_assert(sizeof(Packet) == packet_size);

// sender ip of 0.0.0.0 makes it an rfc 5227 probe
auto build_request(MacAddrRef sender_mac, IPv4Addr sender_ip, IPv4Addr target_ip) -> Packet;
} // namespace net::arp
