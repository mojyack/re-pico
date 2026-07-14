#include "arp.hpp"

#include <noxx/assert.hpp>

namespace net::arp {
namespace {
// packet byte offsets, big-endian scalars
struct Field {
    enum : usize {
        HwType    = 0,
        ProtoType = 2,
        HwLen     = 4,
        ProtoLen  = 5,
        Op        = 6,
        SenderMac = 8,
        SenderIp  = 14,
        TargetMac = 18,
        TargetIp  = 24,
    };
};

constexpr auto hw_type_ethernet = u16(1);
constexpr auto proto_type_ipv4  = u16(0x0800);

auto put_be16(u8* const p, const u16 v) -> void {
    p[0] = v >> 8;
    p[1] = v;
}
} // namespace

auto build_request(const noxx::Span<u8> out, const u8* const sender_mac, const u8* const sender_ip, const u8* const target_ip) -> bool {
    constexpr auto error_value = false;

    ensure(out.size >= packet_size, "arp buffer too small");
    put_be16(out.data + Field::HwType, hw_type_ethernet);
    put_be16(out.data + Field::ProtoType, proto_type_ipv4);
    out[Field::HwLen]    = mac_len;
    out[Field::ProtoLen] = ipv4_len;
    put_be16(out.data + Field::Op, Op::Request);
    noxx::memcpy(out.data + Field::SenderMac, sender_mac, mac_len);
    noxx::memcpy(out.data + Field::SenderIp, sender_ip, ipv4_len);
    for(auto i = usize(0); i < mac_len; i += 1) {
        out[Field::TargetMac + i] = 0;
    }
    noxx::memcpy(out.data + Field::TargetIp, target_ip, ipv4_len);
    return true;
}
} // namespace net::arp
