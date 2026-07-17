#pragma once
#include "mac-addr.hpp"

namespace net {
struct EthernetHeader {
    MacAddr dst;
    MacAddr src;
    u16     ethertype; // big-endian
} __attribute__((packed));

static_assert(sizeof(EthernetHeader) == 14);

struct EtherType {
    enum : u16 {
        IPv4  = 0x0800,
        Arp   = 0x0806,
        Eapol = 0x888e, // 802.1X
    };
};
} // namespace net
