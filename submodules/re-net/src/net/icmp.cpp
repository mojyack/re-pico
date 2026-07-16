#include <noxx/endian.hpp>

#include "ethernet.hpp"
#include "icmp.hpp"
#include "ip.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net::icmp {
auto input(Stack& stack, const IPv4Addr src, AutoPacket packet) -> void {
    if(packet->len < sizeof(EchoHeader)) {
        return;
    }
    auto& header = *(EchoHeader*)packet->data();
    switch(header.type) {
    case Type::EchoRequest: {
        // reflect: flip to a reply and recompute the checksum in place
        header.type     = Type::EchoReply;
        header.code     = 0;
        header.checksum = 0;
        header.checksum = noxx::byteswap(ipv4::checksum({packet->data(), packet->len}));
        ipv4::output(stack, src, ipv4::Proto::Icmp, noxx::move(packet));
    } break;
    case Type::EchoReply:
        if(stack.on_icmp_echo_reply != nullptr) {
            const auto id  = noxx::byteswap(header.id);
            const auto seq = noxx::byteswap(header.seq);
            stack.on_icmp_echo_reply(stack, src, id, seq, {packet->data() + sizeof(EchoHeader), packet->len - sizeof(EchoHeader)});
        }
        break;
    default:
        break;
    }
}

auto send_echo(Stack& stack, const IPv4Addr dst, const u16 id, const u16 seq, const usize payload_len) -> bool {
    constexpr auto error_value = false;

    const auto headroom = sizeof(EthernetHeader) + sizeof(ipv4::Header) + sizeof(EchoHeader);
    auto       packet   = AutoPacket(packet_alloc(headroom));
    ensure(packet.get() != nullptr);

    if(payload_len > 0) {
        const auto data = packet->append(payload_len);
        ensure(data != nullptr, "payload too large");
        for(auto i = usize(0); i < payload_len; i += 1) {
            data[i] = u8(i);
        }
    }

    const auto raw = packet->prepend(sizeof(EchoHeader));
    ensure(raw != nullptr);
    auto& header    = *(EchoHeader*)raw;
    header.type     = Type::EchoRequest;
    header.code     = 0;
    header.checksum = 0;
    header.id       = noxx::byteswap(id);
    header.seq      = noxx::byteswap(seq);
    header.checksum = noxx::byteswap(ipv4::checksum({packet->data(), packet->len}));

    return ipv4::output(stack, dst, ipv4::Proto::Icmp, noxx::move(packet));
}
} // namespace net::icmp
