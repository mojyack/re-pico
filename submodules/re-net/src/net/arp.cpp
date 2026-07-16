#include <noxx/endian.hpp>

#include "arp.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "stack.hpp"

#include <noxx/assert.hpp>

namespace net::arp {
namespace {
constexpr auto hw_type_ethernet = u16(1);

constexpr auto broadcast   = MacAddr{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
constexpr auto all_ones_ip = IPv4Addr{0xff, 0xff, 0xff, 0xff};

auto find(Table& table, const IPv4Addr ip) -> Entry* {
    for(auto& entry : table.entries.data) {
        if(entry.state != State::Free && entry.ip == ip) {
            return &entry;
        }
    }
    return nullptr;
}

// a free slot, or the least-recently-used one evicted
auto alloc(Table& table) -> Entry* {
    auto victim = (Entry*)nullptr;
    for(auto& entry : table.entries.data) {
        if(entry.state == State::Free) {
            return &entry;
        }
        if(victim == nullptr || entry.last_used_ms < victim->last_used_ms) {
            victim = &entry;
        }
    }
    *victim = Entry{}; // drop any held packet / stale mapping
    return victim;
}

// build an arp packet with our addresses as the sender
auto send(Stack& stack, const u16 op, MacAddrRef eth_dst, MacAddrRef arp_target_mac, const IPv4Addr target_ip) -> bool {
    constexpr auto error_value = false;

    auto body = Packet{
        .hw_type    = noxx::byteswap(hw_type_ethernet),
        .proto_type = noxx::byteswap(EtherType::IPv4),
        .hw_len     = MacAddr::size(),
        .proto_len  = sizeof(IPv4Addr),
        .op         = noxx::byteswap(op),
        .sender_ip  = stack.addr,
        .target_ip  = target_ip,
    };
    noxx::memcpy(body.sender_mac.data, stack.netif->mac.data, MacAddr::size());
    noxx::memcpy(body.target_mac.data, arp_target_mac.data, MacAddr::size());

    auto packet = AutoPacket(packet_alloc(sizeof(EthernetHeader)));
    ensure(packet.get() != nullptr);
    unwrap(dst, packet->append(sizeof(body)));
    noxx::memcpy(&dst, &body, sizeof(body));
    return stack.eth_send(eth_dst, EtherType::Arp, noxx::move(packet));
}

auto send_request(Stack& stack, const IPv4Addr target_ip) -> bool {
    return send(stack, Op::Request, broadcast, MacAddr{}, target_ip);
}
} // namespace

auto build_request(const MacAddrRef sender_mac, const IPv4Addr sender_ip, const IPv4Addr target_ip) -> Packet {
    auto ret = Packet{
        .hw_type    = noxx::byteswap(hw_type_ethernet),
        .proto_type = noxx::byteswap(EtherType::IPv4),
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

auto cache(Stack& stack, const IPv4Addr ip, const MacAddrRef mac) -> void {
    auto& table = stack.arp;
    auto  entry = find(table, ip);
    if(entry == nullptr) {
        entry     = alloc(table);
        entry->ip = ip;
    }
    noxx::memcpy(entry->mac.data, mac.data, MacAddr::size());
    entry->state        = State::Resolved;
    entry->retries      = 0;
    entry->deadline_ms  = stack.now_ms + resolved_ttl_ms;
    entry->last_used_ms = stack.now_ms;
    // flush a packet that was waiting on this resolution
    if(entry->pending.get() != nullptr) {
        stack.eth_send(entry->mac, EtherType::IPv4, noxx::move(entry->pending));
    }
}

auto lookup(Table& table, const IPv4Addr ip) -> noxx::Optional<MacAddr> {
    constexpr auto error_value = noxx::nullopt;

    const auto entry = find(table, ip);
    ensure(entry != nullptr && entry->state == State::Resolved);
    return noxx::Array(entry->mac);
}

auto resolve_and_send(Stack& stack, const IPv4Addr next_hop, AutoPacket packet) -> bool {
    // limited broadcast needs no resolution
    if(next_hop == all_ones_ip) {
        return stack.eth_send(broadcast, EtherType::IPv4, noxx::move(packet));
    }

    auto& table = stack.arp;
    if(const auto entry = find(table, next_hop); entry != nullptr) {
        entry->last_used_ms = stack.now_ms;
        if(entry->state == State::Resolved) {
            return stack.eth_send(entry->mac, EtherType::IPv4, noxx::move(packet));
        }
        // already requesting; hold the newest packet
        entry->pending = noxx::move(packet);
        return true;
    }

    auto& entry        = *alloc(table);
    entry              = Entry{};
    entry.ip           = next_hop;
    entry.state        = State::Pending;
    entry.retries      = 1;
    entry.deadline_ms  = stack.now_ms + retry_interval_ms;
    entry.last_used_ms = stack.now_ms;
    entry.pending      = noxx::move(packet);
    return send_request(stack, next_hop);
}

auto input(Stack& stack, AutoPacket packet) -> void {
    if(packet->len < sizeof(Packet)) {
        return;
    }
    const auto& body = *(const Packet*)packet->data();
    if(body.hw_type != noxx::byteswap(hw_type_ethernet) || body.proto_type != noxx::byteswap(EtherType::IPv4)) {
        return;
    }

    // learn the sender regardless of op
    cache(stack, body.sender_ip, body.sender_mac);

    if(noxx::byteswap(body.op) == Op::Request && body.target_ip == stack.addr) {
        send(stack, Op::Reply, body.sender_mac, body.sender_mac, body.sender_ip);
    }
}

auto tick(Stack& stack, const u64 now_ms) -> void {
    for(auto& entry : stack.arp.entries.data) {
        switch(entry.state) {
        case State::Free:
            break;
        case State::Resolved:
            if(now_ms >= entry.deadline_ms) {
                entry = Entry{};
            }
            break;
        case State::Pending:
            if(now_ms >= entry.deadline_ms) {
                if(entry.retries >= max_retries) {
                    entry = Entry{}; // give up; drops the held packet
                } else {
                    entry.retries += 1;
                    entry.deadline_ms = now_ms + retry_interval_ms;
                    send_request(stack, entry.ip);
                }
            }
            break;
        }
    }
}
} // namespace net::arp
