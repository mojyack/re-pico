#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/algorithm.hpp>
#include <noxx/array.hpp>
#include <noxx/defer.hpp>
#include <noxx/format.hpp>

#include "command.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
constexpr auto host_id_seq_mask = u16(0xfff0);

constexpr auto response_timeout_us = u64(600'000);

auto busy = false;
auto seq  = u16(0);

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

    co_unwrap(header, (CommandHeader*)packet->append(sizeof(CommandHeader)));
    header = CommandHeader{
        .flags      = CommandFlag::Req,
        .message_id = id,
        .len        = u16(req.size()),
        .host_id    = host_id,
        .vif_id     = vif,
    };
    co_unwrap(payload, packet->append(req.size()));
    noxx::memcpy(&payload, req.data, req.size());
    co_ensure(co_await yaps_tx(SkbChan::Command, *packet));

    // await the matching response, backlogging unrelated frames meanwhile
    const auto deadline = time::now() + response_timeout_us;
    while(true) {
        co_unwrap(rx, yaps_rx());
        if(!rx) {
            co_ensure(time::now() < deadline, "command response timeout");
            co_await coop::sleep_ms(2);
            continue;
        }
        const auto rcmd = command_payload(*rx);
        if(rcmd == nullptr) {
            push_rx_backlog(rx.get());
            continue;
        }
        if(!(rcmd->flags & CommandFlag::Resp) || rcmd->message_id != id || (rcmd->host_id & host_id_seq_mask) != host_id) {
            log_stray_command(*rcmd);
            continue;
        }
        const auto rresp = (const CommandResponse*)rcmd;
        co_ensure(rcmd->len >= sizeof(rresp->status), "malformed response");
        if(rresp->status != 0) {
            log<"halow: command 0x{04x} failed with status {}\n">(id, i32(rresp->status));
            co_return noxx::nullopt;
        }
        if(resp_vif != nullptr) {
            *resp_vif = rcmd->vif_id;
        }
        const auto copy_len = noxx::min(rcmd->len - sizeof(rresp->status), resp.size());
        noxx::memcpy(resp.data, rresp->data, copy_len);
        co_return usize(copy_len);
    }
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

auto fetch_rx() -> coop::Async<noxx::Optional<net::AutoPacket>> {
    constexpr auto error_value = noxx::nullopt;

    if(auto backlog = pop_rx_backlog(); backlog != nullptr) {
        co_return net::AutoPacket(backlog);
    }
    co_unwrap(rx, yaps_rx());
    if(!rx) {
        co_return net::AutoPacket();
    }
    if(const auto cmd = command_payload(*rx); cmd != nullptr) {
        log_stray_command(*cmd);
        co_return net::AutoPacket();
    }
    co_return move(rx);
}
} // namespace halow
