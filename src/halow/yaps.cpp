#include <coop/ext-event.hpp>
#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/format.hpp>

#include "crc.hpp"
#include "halow.hpp"
#include "util.hpp"
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
constexpr auto tx_space_wake_us    = u64(50'000); // re-check period while waiting for queue space
constexpr auto event_busy_wait_ms  = u64(5);      // polling fallback when an event has another waiter

constexpr auto rx_backlog_max = u32(4);

auto yaps            = YapsTable();
auto ready           = false;
auto rx_backlog_head = (net::Packet*)(nullptr);
auto rx_backlog_len  = u32(0);

// notified by push_rx_backlog for wait_rx
struct RxPendingEvent : coop::ExtEvent {
    auto available() const -> bool override {
        return rx_backlog_len > 0;
    }
};
auto rx_pending_event = RxPendingEvent();

// latched by notify_tx_space (chip pages-freed interrupt), consumed by yaps_tx
auto tx_space_freed = false;

struct TxSpaceEvent : coop::ExtEvent {
    auto available() const -> bool override {
        return tx_space_freed;
    }
};
auto tx_space_event = TxSpaceEvent();

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

auto init_yaps(const YapsTable& table) -> bool {
    constexpr auto error_value = false;

    yaps = table;
    // route the yaps from-chip interrupts to the irq line so rx_task can sleep
    // between frames (ref morse_yaps_hw_enable_irqs)
    constexpr auto irq_bits = u32(Int1::FcPktWaiting | Int1::FcPageFreed);
    unwrap(enabled, read_u32(Reg::Int1En));
    ensure(write_u32(Reg::Int1Clr, irq_bits));
    ensure(write_u32(Reg::Int1En, enabled | irq_bits));
    ready = true;
    return true;
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

auto yaps_tx(const u8 channel, net::Packet& packet, const SkbHeader::TxInfo* const info) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(ready, "yaps not initialized");
    const auto payload_len = u32(packet.len);
    const auto frame_len   = u32(sizeof(SkbHeader) + payload_len + 3) & ~u32(3);
    co_ensure(frame_len + yaps.reserved_page_size <= 0x3fff, "frame too long");
    co_ensure(packet.headroom() >= tx_headroom, "missing tx headroom");
    co_ensure(packet.tailroom() >= frame_len - sizeof(SkbHeader) - payload_len, "missing pad tailroom");

    // frame: [delimiter][skb header][payload][pad to 4]
    for(auto pad = frame_len - sizeof(SkbHeader) - payload_len; pad > 0; pad -= 1) {
        *packet.append(1) = 0;
    }
    const auto hdr = (SkbHeader*)packet.prepend(sizeof(SkbHeader));
    co_ensure(hdr != nullptr);
    *hdr = SkbHeader{
        .sync    = skb_sync,
        .channel = channel,
        .len     = u16(payload_len),
    };
    if(info != nullptr) {
        hdr->tx_info = *info;
    }
    const auto queue = tc_queue_for_channel(channel);
    const auto delim = make_delim(frame_len + yaps.reserved_page_size, queue.pool_id, true);
    noxx::memcpy(packet.prepend(4), &delim, 4);

    // wait for room in the chip queue and its page pool, woken by the
    // pages-freed interrupt (with a periodic re-check as a safety net)
    const auto pages_needed = (frame_len + yaps.reserved_page_size + yaps_page_size - 1) / yaps_page_size + yaps_extra_pages;
    auto       status       = YapsStatus();
    const auto deadline     = time::now() + tx_space_timeout_us;
    while(true) {
        // re-arm before the status read so a page free in between is not lost
        tx_space_freed = false;
        co_ensure(co_await read_status(status));
        co_ensure(status.regs[YapsStatus::TcCrcFail] == 0, "yaps to-chip delimiter crc failure");
        if(status.regs[queue.pkts_reg] < queue.q_size &&
           pages_needed <= status.regs[queue.pool_pages_reg]) {
            break;
        }
        co_ensure(time::now() < deadline, "yaps queue full");
        if(co_await coop::wait_for_event(tx_space_event, tx_space_wake_us) == coop::EventResult::Error) {
            co_await coop::sleep_ms(event_busy_wait_ms); // another tx holds the event
        }
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
    ensure(total % 4 == 0 && total >= sizeof(SkbHeader) && total <= yaps_max_rx, "bad rx frame size");

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

auto parse_skb_header(const net::Packet& packet) -> const SkbHeader* {
    constexpr auto error_value = nullptr;

    const auto hdr = (const SkbHeader*)packet.data();
    ensure(packet.len >= sizeof(SkbHeader) && hdr->sync == skb_sync, "bad skb header");
    ensure(sizeof(SkbHeader) + hdr->offset + hdr->len <= packet.len, "truncated skb frame");
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
    rx_pending_event.notify();
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

auto wait_rx(const u64 timeout_us) -> coop::Async<net::AutoPacket> {
    const auto deadline = time::now() + timeout_us;
    while(true) {
        if(const auto packet = pop_rx_backlog(); packet != nullptr) {
            co_return net::AutoPacket(packet);
        }
        const auto now = time::now();
        if(now >= deadline) {
            co_return net::AutoPacket();
        }
        if(co_await coop::wait_for_event(rx_pending_event, deadline - now) == coop::EventResult::Error) {
            co_await coop::sleep_ms(event_busy_wait_ms); // another task holds the event
        }
    }
}

auto notify_tx_space() -> void {
    tx_space_freed = true;
    tx_space_event.notify();
}
} // namespace halow
