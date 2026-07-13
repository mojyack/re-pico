#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/array.hpp>
#include <noxx/defer.hpp>
#include <noxx/format.hpp>

#include "command.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// command header (ref common/morse_commands.h struct morse_cmd_header)
constexpr auto cmd_hdr_size = u32(12);

// header flags field (ref MORSE_CMD_TYPE_*)
struct CmdFlag {
    enum : u16 {
        Req   = 1 << 0,
        Resp  = 1 << 1,
        Event = 1 << 2,
    };
};

constexpr auto host_id_seq_mask = u16(0xfff0);

constexpr auto response_timeout_us = u64(600'000);

auto busy = false;
auto seq  = u16(0);

// small ring of recent events (id | payload0 << 16), drained by pop_event
constexpr auto event_ring_size = u32(4);
auto           event_ring      = noxx::Array<u32, event_ring_size>();
auto           event_head      = u32(0);
auto           event_count     = u32(0);

// the command payload of a frame, or null if it is not a command frame
auto command_payload(const net::Packet& packet) -> const u8* {
    auto hdr_o = parse_skb_header(packet);
    if(!hdr_o || (*hdr_o).channel != SkbChan::Command || (*hdr_o).len < cmd_hdr_size) {
        return nullptr;
    }
    return packet.data() + skb_hdr_size + (*hdr_o).offset;
}

// consume a command-channel frame that is not the awaited response
auto log_stray_command(const u8* const cmd) -> void {
    const auto flags = get_u16(cmd);
    if(flags & CmdFlag::Event) {
        const auto id  = get_u16(cmd + 2);
        const auto len = get_u16(cmd + 4);
        log<"halow: event 0x{04x} len {}\n">(id, len);
        if(event_count < event_ring_size) {
            const auto payload0 = len >= 1 ? cmd[cmd_hdr_size] : u8(0);
            event_ring[(event_head + event_count) % event_ring_size] = u32(id) | u32(payload0) << 16;
            event_count += 1;
        } else {
            log<"halow: event ring full, dropping event\n">();
        }
    } else {
        log<"halow: stray response 0x{04x}:0x{04x}\n">(get_u16(cmd + 2), get_u16(cmd + 6));
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

    const auto cmd = packet->append(cmd_hdr_size + req.size);
    co_ensure(cmd != nullptr, "command too long");
    put_u16(cmd + 0, CmdFlag::Req);
    put_u16(cmd + 2, id);
    put_u16(cmd + 4, req.size);
    put_u16(cmd + 6, host_id);
    put_u16(cmd + 8, vif);
    put_u16(cmd + 10, 0);
    noxx::memcpy(cmd + cmd_hdr_size, req.data, req.size);
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
        const auto flags = get_u16(rcmd);
        const auto rid   = get_u16(rcmd + 2);
        const auto rlen  = u32(get_u16(rcmd + 4));
        const auto rhid  = get_u16(rcmd + 6);
        if(!(flags & CmdFlag::Resp) || rid != id || (rhid & host_id_seq_mask) != host_id) {
            log_stray_command(rcmd);
            continue;
        }
        co_ensure(rlen >= 4 && skb_hdr_size + cmd_hdr_size + rlen <= rx->len, "malformed response");
        const auto fw_status = get_u32(rcmd + cmd_hdr_size);
        if(fw_status != 0) {
            log<"halow: command 0x{04x} failed with status {}\n">(id, i32(fw_status));
            co_return noxx::nullopt;
        }
        if(resp_vif != nullptr) {
            *resp_vif = get_u16(rcmd + 8);
        }
        const auto data_len = usize(rlen - 4);
        const auto copy_len = data_len < resp.size ? data_len : resp.size;
        noxx::memcpy(resp.data, rcmd + cmd_hdr_size + 4, copy_len);
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
        log_stray_command(cmd);
        co_return net::AutoPacket();
    }
    co_return move(rx);
}
} // namespace halow
