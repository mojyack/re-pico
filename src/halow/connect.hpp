#pragma once
#include <coop/generator.hpp>
#include <net/mac-addr.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

// sta connection: open-auth association and the ethernet-frame datapath
// (ref umac/connection/umac_connection.c, umac/datapath/umac_datapath.c)
namespace halow {
struct LinkStatus {
    u16          vif;
    u16          aid;
    net::MacAddr bssid;
    net::MacAddr mac;
    u32          freq_khz;
    bool         up        = false;
    bool         encrypted = false;
};

// scan for ssid and associate. with an empty password this is open
// authentication; with a password it runs WPA3-SAE (H2E) auth, the RSN 4-way
// handshake and installs the CCMP keys. on success a background maintenance
// task (link_task) is spawned that keeps the link alive, decodes incoming
// data frames and tears the link down on loss
auto connect(const net::MacAddr& mac, noxx::StringView ssid, noxx::StringView password = {}) -> coop::Async<bool>;

// deauthenticate and tear the interface down
auto disconnect() -> coop::Async<bool>;

auto link_status() -> const LinkStatus&;

// true if the yaps rx stream wedged (bad skb header loop); the link was torn
// down and the chip must be rebooted to recover (see D5 notes)
auto link_desynced() -> bool;

// send an ethernet-format frame (dst mac + ethertype + payload) over the link
auto eth_tx(const net::MacAddr& dst, u16 ethertype, noxx::Span<const u8> payload) -> coop::Async<bool>;

// convert a received 802.11 data frame into an ethernet frame in place:
// packet data will start at the 14-byte ethernet header. non-data frames
// (and our own tx status reports) are consumed with a log line instead
auto eth_from_rx(net::Packet& packet) -> bool;

// send a to-ds qos-null keepalive frame to the ap; used by link_task to keep
// the association fresh against the ap's inactivity timeout
auto send_keepalive() -> coop::Async<bool>;

// pop the next decoded ethernet frame the maintenance task has queued, or a
// null packet if none is pending. this is the datapath rx entry point while
// connected (link_task owns the raw yaps stream)
auto link_rx_pop() -> net::AutoPacket;

// background link-maintenance coroutine, spawned by connect as an independent
// task. drains rx, delivers data frames to link_rx_pop, sends periodic
// keepalives, and tears the link down on deauth / beacon loss / rx desync
auto link_task() -> coop::Async<bool>;
} // namespace halow
