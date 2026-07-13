#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/format.hpp>

#include "command.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// command header (ref common/morse_commands.h struct morse_cmd_header)
constexpr auto cmd_hdr_size     = u32(12);
constexpr auto cmd_flag_req     = u16(1 << 0);
constexpr auto cmd_flag_resp    = u16(1 << 1);
constexpr auto cmd_flag_event   = u16(1 << 2);
constexpr auto vif_id_invalid   = u16(0xffff);
constexpr auto host_id_seq_mask = u16(0xfff0);

constexpr auto response_timeout_us = u64(600'000);

auto busy = false;
auto seq  = u16(0);

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

auto get_u32(const u8* const p) -> u32 {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

auto put_u16(u8* const p, const u16 v) -> void {
    p[0] = v;
    p[1] = v >> 8;
}

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
    if(flags & cmd_flag_event) {
        log<"halow: event 0x{04x} len {}\n">(get_u16(cmd + 2), get_u16(cmd + 4));
    } else {
        log<"halow: stray response 0x{04x}:0x{04x}\n">(get_u16(cmd + 2), get_u16(cmd + 6));
    }
}
} // namespace

auto send_command(const u16 id, const noxx::Span<const u8> req, const noxx::Span<u8> resp) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    co_ensure(!busy, "command already in flight");
    struct BusyGuard {
        BusyGuard() { busy = true; }
        ~BusyGuard() { busy = false; }
    } busy_guard;

    seq                = seq % 0xfff + 1;
    const auto host_id = u16(seq << 4);

    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);

    const auto cmd = packet->append(cmd_hdr_size + req.size);
    co_ensure(cmd != nullptr, "command too long");
    put_u16(cmd + 0, cmd_flag_req);
    put_u16(cmd + 2, id);
    put_u16(cmd + 4, req.size);
    put_u16(cmd + 6, host_id);
    put_u16(cmd + 8, vif_id_invalid);
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
        if(!(flags & cmd_flag_resp) || rid != id || (rhid & host_id_seq_mask) != host_id) {
            log_stray_command(rcmd);
            continue;
        }
        co_ensure(rlen >= 4 && skb_hdr_size + cmd_hdr_size + rlen <= rx->len, "malformed response");
        const auto fw_status = get_u32(rcmd + cmd_hdr_size);
        if(fw_status != 0) {
            log<"halow: command 0x{04x} failed with status {}\n">(id, i32(fw_status));
            co_return noxx::nullopt;
        }
        const auto data_len = usize(rlen - 4);
        const auto copy_len = data_len < resp.size ? data_len : resp.size;
        noxx::memcpy(resp.data, rcmd + cmd_hdr_size + 4, copy_len);
        co_return usize(copy_len);
    }
}

auto fetch_rx() -> coop::Async<noxx::Optional<net::AutoPacket>> {
    constexpr auto error_value = noxx::nullopt;

    if(auto backlog = pop_rx_backlog(); backlog != nullptr) {
        co_return net::AutoPacket();
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
