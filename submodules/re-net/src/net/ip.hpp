#pragma once
#include <net/mac-addr.hpp>
#include <net/packet.hpp>
#include <noxx/array.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

#include <noxx/bytes-alias.hpp>

namespace net {
bytes_alias(IPv4Addr, 4);

struct Stack;

auto parse_ip(noxx::StringView str) -> noxx::Optional<IPv4Addr>;
} // namespace net

// internet protocol v4 (rfc 791)
namespace net::ipv4 {
// ipv4 header without options; multi-byte scalars are big-endian
struct Header {
    u8       version_ihl; // 0x45 for version 4, 5-word header
    u8       dscp_ecn;
    u16      total_len; // header + payload
    u16      id;
    u16      flags_frag; // we set DF, never fragment
    u8       ttl;
    u8       proto;
    u16      checksum; // over the header only
    IPv4Addr src;
    IPv4Addr dst;
} __attribute__((packed));
static_assert(sizeof(Header) == 20);

struct Proto {
    enum : u8 {
        Icmp = 1,
        Tcp  = 6,
        Udp  = 17,
    };
};

// rfc 1071 ones-complement sum, folded to 16 bits (host order return)
auto checksum(noxx::Span<const u8> data) -> u16;

// handle a received ipv4 frame (eth header already consumed); src_mac is the
// ethernet source, learned into the arp cache
auto input(Stack& stack, MacAddrRef src_mac, AutoPacket packet) -> void;

// prepend an ipv4 header and send toward dst; picks gateway vs on-link and
// hands off to arp. the packet's data() must point at the l4 payload with
// sizeof(Header) headroom available.
auto output(Stack& stack, IPv4Addr dst, u8 proto, AutoPacket packet) -> bool;
} // namespace net::ipv4

#include <noxx/bytes-alias.hpp>

// formatting support
#include <noxx/comptime-string.hpp>
#include <noxx/format/integer.hpp>
#include <noxx/string.hpp>

#include <noxx/assert.hpp>

namespace noxx::fmt {
template <comptime::String params>
auto format_segment(String& result, const net::IPv4AddrRef var) -> bool {
    constexpr auto error_value = false;
    static_assert(params.size() == 0, "format patameters not supported");
    for(auto i = 0uz; i < net::IPv4Addr::size(); i += 1) {
        ensure(format_segment<comptime::String("d")>(result, var[i]));
        ensure(result.append('.'));
    }
    result.resize(result.size() - 1); // remove last '.'
    return true;
}
} // namespace noxx::fmt

#include <noxx/assert.hpp>
