#include <coop/ext-event.hpp>
#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <net/packet-buf.hpp>
#include <noxx/algorithm.hpp>
#include <noxx/array.hpp>
#include <noxx/defer.hpp>
#include <noxx/format.hpp>

#include "command.hpp"
#include "dot11.hpp"
#include "halow.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
constexpr auto host_id_seq_mask = u16(0xfff0);

constexpr auto response_timeout_us = u64(600'000);

constexpr auto rx_error_limit      = u32(8); // consecutive rx stream errors treated as a desync
constexpr auto rx_error_backoff_ms = u64(5); // breather between rx errors, the irq line stays asserted

auto busy     = false;
auto seq      = u16(0);
auto desynced = false; // rx stream wedged, chip reboot required

// the response send_command is waiting for, filled by rx_task
struct PendingResponse {
    u16            id;
    u16            host_id;
    noxx::Span<u8> resp;
    usize          copy_len = 0;
    u16            vif      = 0;
    i32            status   = 0;
    bool           done     = false;
};

auto pending_response = (PendingResponse*)nullptr;

struct ResponseEvent : coop::ExtEvent {
    auto available() const -> bool override {
        return pending_response != nullptr && pending_response->done;
    }
};
auto response_event = ResponseEvent();

// small ring of recent events (id | payload0 << 16), drained by pop_event
constexpr auto event_ring_size = u32(4);
auto           event_ring      = noxx::Array<u32, event_ring_size>();
auto           event_head      = u32(0);
auto           event_count     = u32(0);

// the command header of a frame, or null if it is not a command frame;
// the header's own len field is validated against the skb frame
auto command_payload(const net::Packet& packet) -> const CommandHeader* {
    const auto skbh = parse_skb_header(packet);
    if(skbh == nullptr || skbh->channel != SkbChan::Command || skbh->len < sizeof(CommandHeader)) {
        return nullptr;
    }
    const auto header = (const CommandHeader*)packet_frame(packet, *skbh);
    if(sizeof(CommandHeader) + header->len > skbh->len) {
        return nullptr;
    }
    return header;
}

// s1g beacons stream in continuously once a channel is set and nobody
// consumes them; drop them at the demux point so they cannot crowd the
// bounded rx backlog and the packet pool out of useful frames
auto is_s1g_beacon(const net::Packet& packet) -> bool {
    const auto skbh = parse_skb_header(packet);
    if(skbh == nullptr || skbh->channel == SkbChan::TxStatus || skbh->len < sizeof(u16)) {
        return false;
    }
    const auto body = packet_frame(packet, *skbh);
    const auto fc   = u16(body[0]) | u16(body[1]) << 8;
    return (fc & dot11::Fc::VerTypeSubMask) == dot11::Fc::S1gBeacon;
}

// log a tx status report frame, they carry no payload for anyone to consume
// (ref struct morse_skb_tx_status)
auto log_tx_status(const net::Packet& packet, const SkbHeader& hdr) -> void {
    if(hdr.len < sizeof(SkbHeader::TxStatus)) {
        return;
    }
    const auto status = (const SkbHeader::TxStatus*)packet_frame(packet, hdr);
    log<"halow: tx status pkt {} {}\n">(status->pkt_id, (status->flags & TxFlag::NoAck) ? "no-ack" : "acked");
}

// consume a command-channel frame that is not the awaited response
auto log_stray_command(const CommandHeader& header) -> void {
    if(header.flags & CommandFlag::Event) {
        log<"halow: event 0x{04x} len {}\n">(header.message_id, header.len);
        if(event_count < event_ring_size) {
            const auto payload0 = header.len >= 1 ? *(const u8*)(&header + 1) : u8(0);

            event_ring[(event_head + event_count) % event_ring_size] = u32(header.message_id) | u32(payload0) << 16;
            event_count += 1;
        } else {
            log<"halow: event ring full, dropping event\n">();
        }
    } else {
        log<"halow: stray response 0x{04x}:0x{04x}\n">(header.message_id, header.host_id);
    }
}

// route a command-channel frame: the awaited response fills the pending slot,
// events land in the ring, anything else is logged and dropped
auto handle_command_frame(const CommandHeader& header) -> void {
    if(pending_response == nullptr || !(header.flags & CommandFlag::Resp) ||
       header.message_id != pending_response->id ||
       (header.host_id & host_id_seq_mask) != pending_response->host_id) {
        log_stray_command(header);
        return;
    }
    auto&      p    = *pending_response;
    const auto resp = (const CommandResponse*)&header;
    if(header.len < sizeof(resp->status)) {
        log<"halow: malformed response 0x{04x}\n">(header.message_id);
        return;
    }
    p.status   = resp->status;
    p.vif      = header.vif_id;
    p.copy_len = noxx::min(header.len - sizeof(resp->status), p.resp.size());
    noxx::memcpy(p.resp.data, resp->data, p.copy_len);
    p.done = true;
    response_event.notify();
}

// one from-chip frame out of the yaps stream, demultiplexed
auto dispatch_rx(net::AutoPacket packet) -> void {
    if(const auto cmd = command_payload(*packet); cmd != nullptr) {
        handle_command_frame(*cmd);
        return;
    }
    if(const auto skbh = parse_skb_header(*packet); skbh != nullptr && skbh->channel == SkbChan::TxStatus) {
        log_tx_status(*packet, *skbh);
        return;
    }
    if(is_s1g_beacon(*packet)) {
        return;
    }
    push_rx_backlog(packet.release());
}
} // namespace

auto send_command(const u16 id, const noxx::Span<const u8> req, const noxx::Span<u8> resp, const u16 vif, u16* const resp_vif) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    co_ensure(!busy, "command already in flight");
    busy = true;
    defer { busy = false; };

    seq                = seq % 0xfff + 1;
    const auto host_id = u16(seq << 4);

    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);

    auto w = net::PacketWriter(*packet);
    co_ensure(w.append_obj(CommandHeader{
        .flags      = CommandFlag::Req,
        .message_id = id,
        .len        = u16(req.size()),
        .host_id    = host_id,
        .vif_id     = vif,
    }));
    co_ensure(w.append_span(req));

    // arm the response slot before the send; yaps_tx suspends, the response
    // may already be dispatched while it waits for queue space
    auto pending = PendingResponse{
        .id      = id,
        .host_id = host_id,
        .resp    = resp,
    };
    pending_response = &pending;
    defer { pending_response = nullptr; };

    co_ensure(co_await yaps_tx(SkbChan::Command, *packet));

    co_await coop::wait_for_event(response_event, response_timeout_us);
    co_ensure(pending.done, "command response timeout");
    if(pending.status != 0) {
        log<"halow: command 0x{04x} failed with status {}\n">(id, i32(pending.status));
        co_return noxx::nullopt;
    }
    if(resp_vif != nullptr) {
        *resp_vif = pending.vif;
    }
    co_return usize(pending.copy_len);
}

auto pop_event() -> noxx::Optional<u32> {
    if(event_count == 0) {
        return noxx::nullopt;
    }
    auto id    = event_ring[event_head];
    event_head = (event_head + 1) % event_ring_size;
    event_count -= 1;
    return id;
}

auto rx_desynced() -> bool {
    return desynced;
}

auto rx_task() -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto rx_errors = u32(0);
    while(true) {
        co_await coop::wait_for_event(chip_irq_event);

        // ack the chip interrupt before draining: frames arriving mid-drain
        // re-assert the (level-held) irq line and wake us again
        const auto sts_o = read_u32(Reg::Int1Sts);
        if(!sts_o || !write_u32(Reg::Int1Clr, *sts_o)) {
            log<"halow: chip irq status access failed\n">();
            co_await coop::sleep_ms(rx_error_backoff_ms);
            continue;
        }
        if(*sts_o & Int1::FcPageFreed) {
            notify_tx_space();
        }

        // drain the from-chip stream
        while(true) {
            auto rx_o = yaps_rx();
            if(!rx_o) {
                // a run of rx errors means the stream desynced; stop instead of
                // spinning on the stubbornly asserted irq line
                rx_errors += 1;
                if(rx_errors >= rx_error_limit) {
                    desynced = true;
                    co_ensure(false, "rx stream desynced (reboot required)");
                }
                co_await coop::sleep_ms(rx_error_backoff_ms);
                break;
            }
            if(!*rx_o) {
                break; // stream empty
            }
            rx_errors = 0;
            dispatch_rx(noxx::move(*rx_o));
        }
    }
}
} // namespace halow
