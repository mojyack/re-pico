#pragma once
#include <coop/promise.hpp>
#include <coop/single-event.hpp>
#include <net/ip.hpp>
#include <net/udp.hpp>
#include <noxx/int.hpp>

// dhcp client (rfc 2131)
namespace net {
struct Stack;
} // namespace net

namespace net::dhcp {
constexpr auto server_port = u16(67);
constexpr auto client_port = u16(68);

// protocol timing
constexpr auto reply_timeout_ms = u64(4'000); // per-attempt reply wait
constexpr auto max_tries        = u32(4);     // attempts per exchange before giving up
constexpr auto default_lease_s  = u32(120);   // assumed lease when the server sends none

constexpr auto magic_cookie = u32(0x63825363);

struct Op {
    enum : u8 {
        BootRequest = 1,
        BootReply   = 2,
    };
};

struct HType {
    enum : u8 {
        Ethernet = 1,
    };
};

struct MessageType {
    enum : u8 {
        Discover = 1,
        Offer    = 2,
        Request  = 3,
        Decline  = 4,
        Ack      = 5,
        Nak      = 6,
        Release  = 7,
        Inform   = 8,
    };
};

struct Option {
    enum : u8 {
        Pad          = 0,
        SubnetMask   = 1,
        Router       = 3,
        Dns          = 6,
        RequestedIp  = 50,
        LeaseTime    = 51,
        MessageType  = 53,
        ServerId     = 54,
        ParamReqList = 55,
        RenewalTime  = 58,
        End          = 255,
    };
};

// fixed-size message head, through the options magic cookie; multi-byte
// scalars are big-endian
struct Header {
    u8       op;
    u8       htype;
    u8       hlen;
    u8       hops;
    u32      xid;
    u16      secs;
    u16      flags;
    IPv4Addr ciaddr;
    IPv4Addr yiaddr;
    IPv4Addr siaddr;
    IPv4Addr giaddr;
    u8       chaddr[16];
    u8       sname[64];
    u8       file[128];
    u32      cookie;
} __attribute__((packed));
static_assert(sizeof(Header) == 240);

enum class State : u8 {
    Idle,       // task not running
    Selecting,  // discover sent, awaiting an offer
    Requesting, // request sent, awaiting an ack
    Bound,      // lease held
    Renewing,   // unicast request sent at t1, awaiting an ack
};

// lease parameters as parsed from an offer/ack
struct Lease {
    IPv4Addr addr      = {};
    IPv4Addr netmask   = {};
    IPv4Addr router    = {};
    IPv4Addr server_id = {};
    u32      lease_s   = 0;
};

struct Client {
    State       state  = State::Idle;
    u32         xid    = 0;
    Lease       lease  = {};
    udp::Socket socket = {};

    // set by the socket rx hook when a state-advancing reply arrives
    u8                reply_type  = 0;
    Lease             reply_lease = {};
    coop::SingleEvent reply;

    // acquire a lease, configure the stack, and keep the lease renewed;
    // push onto the runner after the stack is up. loops forever
    auto task(Stack& stack) -> coop::Async<void>;
};
} // namespace net::dhcp
