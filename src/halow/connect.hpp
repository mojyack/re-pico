#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

#include "dot11.hpp"

// sta connection: open-auth association and the ethernet-frame datapath
// (ref umac/connection/umac_connection.c, umac/datapath/umac_datapath.c)
namespace halow {
struct LinkStatus {
    u16            vif;
    u16            aid;
    dot11::MacAddr bssid;
    dot11::MacAddr mac;
    u32            freq_khz;
    bool           up = false;
};

// scan for ssid and associate with open authentication
auto connect(const dot11::MacAddr& mac, noxx::StringView ssid) -> coop::Async<bool>;

// deauthenticate and tear the interface down
auto disconnect() -> coop::Async<bool>;

auto link_status() -> const LinkStatus&;

// send an ethernet-format frame (dst mac + ethertype + payload) over the link
auto eth_tx(const dot11::MacAddr& dst, u16 ethertype, noxx::Span<const u8> payload) -> coop::Async<bool>;

// convert a received 802.11 data frame into an ethernet frame in place:
// packet data will start at the 14-byte ethernet header. non-data frames
// (and our own tx status reports) are consumed with a log line instead
auto eth_from_rx(net::Packet& packet) -> bool;
} // namespace halow
