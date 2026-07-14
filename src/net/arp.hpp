#pragma once
#include <noxx/int.hpp>
#include <noxx/span.hpp>

// address resolution protocol over ethernet/ipv4 (rfc 826)
namespace net::arp {
constexpr auto ethertype   = u16(0x0806);
constexpr auto mac_len     = usize(6);
constexpr auto ipv4_len    = usize(4);
constexpr auto packet_size = usize(28); // fixed for the ethernet/ipv4 pair

struct Op {
    enum : u16 {
        Request = 1,
        Reply   = 2,
    };
};

// write an arp request into out (needs packet_size bytes); target mac is
// zero, a sender ip of 0.0.0.0 makes it an rfc 5227 probe
auto build_request(noxx::Span<u8> out, const u8* sender_mac, const u8* sender_ip, const u8* target_ip) -> bool;
} // namespace net::arp
