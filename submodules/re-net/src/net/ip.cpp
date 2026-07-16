#include <noxx/charconv.hpp>
#include <noxx/endian.hpp>

#include "arp.hpp"
#include "icmp.hpp"
#include "ip.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net {
auto parse_ip(const noxx::StringView str) -> noxx::Optional<IPv4Addr> {
    constexpr auto error_value = noxx::nullopt;

    auto ret  = IPv4Addr();
    auto rest = str;
    for(auto i = usize(0); i < 4; i += 1) {
        const auto last = i == 3;
        const auto dot  = rest.find(".");
        ensure(last ? dot < 0 : dot >= 0, "bad ipv4 address");
        unwrap(v, noxx::from_chars<u8>(last ? rest : rest.substr(0, dot)));
        ret[i] = v;
        if(!last) {
            rest = rest.substr(dot + 1);
        }
    }
    return ret;
}
} // namespace net

namespace net::ipv4 {
namespace {
constexpr auto default_ttl = u8(64);
constexpr auto flag_df     = u16(0x4000);

auto ident = u16(0); // monotonic ipv4 id counter
} // namespace

auto checksum(const noxx::Span<const u8> data) -> u16 {
    auto sum = u32(0);
    for(auto i = usize(0); i < data.size(); i += 2) {
        const auto hi = u32(data[i]);
        const auto lo = i + 1 < data.size() ? u32(data[i + 1]) : u32(0);
        sum += (hi << 8) | lo;
    }
    while((sum >> 16) != 0) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return u16(~sum);
}

auto input(Stack& stack, const MacAddrRef src_mac, AutoPacket packet) -> coop::Async<void> {
    if(packet->len < sizeof(Header)) {
        co_return;
    }
    const auto& header = *(const Header*)packet->data();
    if((header.version_ihl >> 4) != 4) {
        co_return;
    }
    const auto ihl = usize(header.version_ihl & 0x0f) * 4;
    if(ihl < sizeof(Header) || packet->len < ihl) {
        co_return;
    }
    if(checksum({packet->data(), ihl}) != 0) {
        co_return; // corrupt header
    }

    // learn the l2/l3 mapping from any inbound frame
    co_await arp::cache(stack, header.src, src_mac);

    // accept only frames addressed to us
    if(!(header.dst == stack.addr)) {
        co_return;
    }

    const auto total = usize(noxx::byteswap(header.total_len));
    if(total < ihl || total > packet->len) {
        co_return;
    }
    const auto src   = header.src;
    const auto proto = header.proto;
    packet->len      = u16(total);
    packet->consume(ihl);

    switch(proto) {
    case Proto::Icmp:
        co_await icmp::input(stack, src, noxx::move(packet));
        break;
    default:
        break; // udp/tcp arrive in later phases
    }
}

auto output(Stack& stack, const IPv4Addr dst, const u8 proto, AutoPacket packet) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto payload = packet->len;
    const auto raw     = packet->prepend(sizeof(Header));
    co_ensure(raw != nullptr, "no headroom for ipv4 header");

    auto& header       = *(Header*)raw;
    header             = Header{};
    header.version_ihl = 0x45;
    header.total_len   = noxx::byteswap(u16(sizeof(Header) + payload));
    header.id          = noxx::byteswap(ident++);
    header.flags_frag  = noxx::byteswap(flag_df);
    header.ttl         = default_ttl;
    header.proto       = proto;
    header.src         = stack.addr;
    header.dst         = dst;
    header.checksum    = noxx::byteswap(checksum({raw, sizeof(Header)}));

    const auto next_hop = stack.on_link(dst) ? dst : stack.gateway;
    co_return co_await arp::resolve_and_send(stack, next_hop, noxx::move(packet));
}
} // namespace net::ipv4
