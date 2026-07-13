#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/format.hpp>

#include "crc.hpp"
#include "halow.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// yaps stream framing (ref morse_driver/mm8108/yaps-hw.c)
constexpr auto yaps_page_size   = u32(256);
constexpr auto yaps_extra_pages = u32(2); // metadata page + phandle-corruption workaround page
constexpr auto yaps_max_rx      = u32(1628);

constexpr auto tx_space_timeout_us = u64(600'000);
constexpr auto status_timeout_us   = u64(100'000);

constexpr auto rx_backlog_max = u32(4);

auto yaps            = YapsTable();
auto ready           = false;
auto rx_backlog_head = (net::Packet*)(nullptr);
auto rx_backlog_len  = u32(0);

template <noxx::comptime::String str, class... Args>
auto log(const Args&... args) -> void {
    auto raw = noxx::format<str>(noxx::move(args)...);
    if(raw) {
        noxx::console_out((*raw).data());
    }
}

auto get_u16(const u8* const p) -> u16 {
    return u16(p[0]) | u16(p[1]) << 8;
}

auto put_u16(u8* const p, const u16 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
}

auto put_u32(u8* const p, const u32 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

// crc7 over the delimiter payload bits, big-endian byte order (ref morse_yaps_crc)
auto delim_crc(const u32 word) -> u8 {
    const auto masked  = word & 0x1ffffff;
    const u8   bytes[] = {u8(masked >> 24), u8(masked >> 16), u8(masked >> 8), u8(masked)};
    return crc7(bytes, sizeof(bytes));
}

auto make_delim(const u32 pkt_size, const u32 pool, const bool irq) -> u32 {
    const auto pad = (4 - pkt_size % 4) % 4;
    const auto d   = (pkt_size & 0x3fff) | pool << 14 | pad << 17 | u32(irq) << 19;
    return d | u32(delim_crc(d)) << 25;
}

// to-chip queue selection and its flow-control counters (ref morse_yaps_tx)
struct TcQueue {
    u32 pool_id;
    u32 pool_pages_reg;
    u32 pkts_reg;
    u32 q_size;
};

auto tc_queue_for_channel(const u8 channel) -> TcQueue {
    switch(channel) {
    case SkbChan::Command:
        return {1, YapsStatus::TcCmdPoolPages, YapsStatus::TcCmdPkts, yaps.tc_cmd_q_size};
    case SkbChan::Beacon:
        return {2, YapsStatus::TcBeaconPoolPages, YapsStatus::TcBeaconPkts, yaps.tc_beacon_q_size};
    case SkbChan::Mgmt:
        return {3, YapsStatus::TcMgmtPoolPages, YapsStatus::TcMgmtPkts, yaps.tc_mgmt_q_size};
    default: // data, ndp, data-noack, loopback
        return {0, YapsStatus::TcTxPoolPages, YapsStatus::TcTxPkts, yaps.tc_tx_q_size};
    }
}
} // namespace

auto init_yaps(const YapsTable& table) -> void {
    yaps  = table;
    ready = true;
}

auto read_status(YapsStatus& status) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(ready, "yaps not initialized");
    const auto deadline = time::now() + status_timeout_us;
    while(true) {
        co_ensure(read_multi(yaps.status_regs_addr, (u8*)status.regs, sizeof(status.regs)));
        if(status.regs[YapsStatus::Lock] == 0) {
            co_return true;
        }
        co_ensure(time::now() < deadline, "yaps status lock timeout");
        co_await coop::sleep_ms(1);
    }
}

auto yaps_tx(const u8 channel, net::Packet& packet) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(ready, "yaps not initialized");
    const auto payload_len = u32(packet.len);
    const auto frame_len   = (skb_hdr_size + payload_len + 3) & ~u32(3);
    co_ensure(frame_len + yaps.reserved_page_size <= 0x3fff, "frame too long");
    co_ensure(packet.headroom() >= tx_headroom, "missing tx headroom");
    co_ensure(packet.tailroom() >= frame_len - skb_hdr_size - payload_len, "missing pad tailroom");

    // frame: [delimiter][skb header, tx_info zeroed][payload][pad to 4]
    for(auto pad = frame_len - skb_hdr_size - payload_len; pad > 0; pad -= 1) {
        *packet.append(1) = 0;
    }
    const auto hdr = packet.prepend(skb_hdr_size);
    co_ensure(hdr != nullptr);
    for(auto i = u32(0); i < skb_hdr_size; i += 1) {
        hdr[i] = 0;
    }
    hdr[0] = skb_sync;
    hdr[1] = channel;
    put_u16(hdr + 2, payload_len);
    const auto queue = tc_queue_for_channel(channel);
    put_u32(packet.prepend(4), make_delim(frame_len + yaps.reserved_page_size, queue.pool_id, true));

    // wait for room in the chip queue and its page pool
    const auto pages_needed = (frame_len + yaps.reserved_page_size + yaps_page_size - 1) / yaps_page_size + yaps_extra_pages;
    auto       status       = YapsStatus();
    const auto deadline     = time::now() + tx_space_timeout_us;
    while(true) {
        co_ensure(co_await read_status(status));
        co_ensure(status.regs[YapsStatus::TcCrcFail] == 0, "yaps to-chip delimiter crc failure");
        if(status.regs[queue.pkts_reg] < queue.q_size &&
           pages_needed <= status.regs[queue.pool_pages_reg]) {
            break;
        }
        co_ensure(time::now() < deadline, "yaps queue full");
        co_await coop::sleep_ms(5);
    }

    // the stream write alone hands the frame to the firmware, no doorbell
    co_ensure(write_multi(yaps.yds_addr, packet.data(), packet.len));
    co_return true;
}

auto yaps_rx() -> noxx::Optional<net::AutoPacket> {
    constexpr auto error_value = noxx::nullopt;

    ensure(ready, "yaps not initialized");
    unwrap(delim, read_u32(yaps.ysl_addr));
    if(delim == 0) {
        return net::AutoPacket();
    }
    ensure(delim_crc(delim) == delim >> 25, "rx delimiter crc mismatch");
    const auto raw_size = delim & 0x3fff;
    const auto padding  = (delim >> 17) & 0x3;
    ensure(raw_size > yaps.reserved_page_size, "rx delimiter too small");
    const auto total = raw_size - yaps.reserved_page_size + padding;
    ensure(total % 4 == 0 && total >= skb_hdr_size && total <= yaps_max_rx, "bad rx frame size");

    auto packet = net::AutoPacket(net::packet_alloc());
    if(!packet) {
        // we cannot keep the payload,
        // but it must be drained or the host and chip disagree on stream position
        static auto scratch = noxx::Array<u8, yaps_max_rx>();
        ensure(read_multi(yaps.ysl_addr + 4, scratch.data, total));
        ensure(false, "rx dropped, packet pool exhausted");
    }
    const auto buf = packet->append(total);
    if(buf == nullptr || !read_multi(yaps.ysl_addr + 4, buf, total)) {
        ensure(false, "rx frame read failed");
    }
    if(packet->data()[0] != skb_sync) {
        ensure(false, "rx skb sync mismatch");
    }
    return packet;
}

auto parse_skb_header(const net::Packet& packet) -> noxx::Optional<SkbHeader> {
    constexpr auto error_value = noxx::nullopt;

    const auto p = packet.data();
    ensure(packet.len >= skb_hdr_size && p[0] == skb_sync, "bad skb header");
    auto hdr        = SkbHeader();
    hdr.channel     = p[1];
    hdr.len         = get_u16(p + 2);
    hdr.offset      = p[4];
    hdr.rssi        = get_u16(p + 16);
    hdr.freq_100khz = get_u16(p + 18);
    ensure(skb_hdr_size + hdr.offset + hdr.len <= packet.len, "truncated skb frame");
    return hdr;
}

auto push_rx_backlog(net::Packet* const packet) -> void {
    if(rx_backlog_len >= rx_backlog_max) {
        log<"halow: rx backlog full, dropping frame\n">();
        net::packet_free(packet);
        return;
    }
    packet->next = nullptr;
    auto tail    = &rx_backlog_head;
    while(*tail != nullptr) {
        tail = &(*tail)->next;
    }
    *tail = packet;
    rx_backlog_len += 1;
}

auto pop_rx_backlog() -> net::Packet* {
    const auto packet = rx_backlog_head;
    if(packet != nullptr) {
        rx_backlog_head = packet->next;
        packet->next    = nullptr;
        rx_backlog_len -= 1;
    }
    return packet;
}
} // namespace halow
