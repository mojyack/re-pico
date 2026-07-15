#include <noxx/endian.hpp>

#include "arp.hpp"

#include <noxx/assert.hpp>

namespace net::arp {
namespace {
constexpr auto hw_type_ethernet = u16(1);
constexpr auto proto_type_ipv4  = u16(0x0800);
} // namespace

auto build_request(const MacAddrRef sender_mac, const IPv4Addr sender_ip, const IPv4Addr target_ip) -> Packet {
    auto ret = Packet{
        .hw_type    = noxx::byteswap(hw_type_ethernet),
        .proto_type = noxx::byteswap(proto_type_ipv4),
        .hw_len     = MacAddr::size(),
        .proto_len  = sizeof(IPv4Addr),
        .op         = noxx::byteswap(u16(Op::Request)),
        .sender_ip  = sender_ip,
        .target_ip  = target_ip,
    };
    noxx::memcpy(ret.sender_mac.data, sender_mac.data, sender_mac.size());
    noxx::memset(ret.target_mac.data, 0, sender_mac.size());
    return ret;
}
} // namespace net::arp
