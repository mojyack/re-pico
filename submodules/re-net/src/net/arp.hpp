#pragma once
#include <net/ip.hpp>
#include <net/mac-addr.hpp>
#include <net/packet.hpp>
#include <noxx/array.hpp>
#include <noxx/int.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

// address resolution protocol over ethernet/ipv4 (rfc 826)
namespace net {
struct Stack;
} // namespace net

namespace net::arp {
// cache tuning
constexpr auto table_size        = usize(8);
constexpr auto resolved_ttl_ms   = u64(60'000); // resolved entry lifetime
constexpr auto retry_interval_ms = u64(1'000);  // pending re-request period
constexpr auto max_retries       = u32(3);      // give up after this many requests

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
static_assert(sizeof(Packet) == 28);

enum class State : u8 {
    Free,     // slot unused
    Pending,  // request sent, awaiting reply
    Resolved, // mac known
};

struct Entry {
    IPv4Addr   ip           = {};
    MacAddr    mac          = {};
    State      state        = State::Free;
    u32        retries      = 0;
    u64        deadline_ms  = 0;  // resolved: expiry; pending: next re-request
    u64        last_used_ms = 0;  // for lru reuse
    AutoPacket pending      = {}; // one packet held until resolution
};

struct Table {
    noxx::Array<Entry, table_size> entries = {};
};

// sender ip of 0.0.0.0 makes it an rfc 5227 probe
auto build_request(MacAddrRef sender_mac, IPv4Addr sender_ip, IPv4Addr target_ip) -> Packet;

// handle a received arp packet (eth header already consumed); replies to
// requests for our ip and learns sender mappings, flushing pending packets
auto input(Stack& stack, AutoPacket packet) -> void;

// look up a resolved mac; nullopt if unknown or still pending
auto lookup(Table& table, IPv4Addr ip) -> noxx::Optional<MacAddr>;

// resolve next_hop and send the ipv4 packet; queues it and requests the mac if
// not yet known (one packet held per entry, replacing any prior)
auto resolve_and_send(Stack& stack, IPv4Addr next_hop, AutoPacket packet) -> bool;

// learn a mapping (e.g. from an inbound ipv4 frame's ethernet source)
auto cache(Stack& stack, IPv4Addr ip, MacAddrRef mac) -> void;

// re-request pending entries and age out resolved ones
auto tick(Stack& stack, u64 now_ms) -> void;
} // namespace net::arp
