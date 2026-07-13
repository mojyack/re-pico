#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/format.hpp>
#include <noxx/unique-ptr.hpp>

#include "command.hpp"
#include "crc.hpp"
#include "halow.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// morse skb framing, prepended to every yaps frame (ref morse_driver/skb_header.h)
constexpr auto skb_hdr_size = u32(40); // 8 byte header + 32 byte tx/rx info union
constexpr auto skb_sync     = u8(0xAA);
constexpr auto skb_chan_cmd = u8(0xFE); // MORSE_SKB_CHAN_COMMAND

// command header (ref common/morse_commands.h struct morse_cmd_header)
constexpr auto cmd_hdr_size   = u32(12);
constexpr auto cmd_flag_req   = u16(1 << 0);
constexpr auto cmd_flag_resp  = u16(1 << 1);
constexpr auto cmd_flag_event = u16(1 << 2);
constexpr auto vif_id_invalid = u16(0xffff);
constexpr auto host_id_seq_mask = u16(0xfff0);

// yaps stream framing (ref morse_driver/mm8108/yaps-hw.c)
constexpr auto yaps_page_size   = u32(256);
constexpr auto yaps_extra_pages = u32(2); // metadata page + phandle-corruption workaround page
constexpr auto yaps_pool_cmd    = u32(1); // MORSE_YAPS_CMD_Q
constexpr auto yaps_max_rx      = u32(1628);

constexpr auto response_timeout_us = u64(600'000);
constexpr auto status_timeout_us   = u64(100'000);

// yaps status register block, read as one chunk (ref struct morse_yaps_status_registers)
struct Status {
    enum : u32 {
        TcCmdPoolPages = 1,
        TcCmdPkts      = 9,
        FcPkts         = 12,
        Lock           = 17,
        Count          = 18,
    };
    u32 regs[Count];
};

auto yaps  = YapsTable();
auto ready = false;
auto busy  = false;
auto seq   = u16(0);

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
    const auto d = (pkt_size & 0x3fff) | pool << 14 | u32(irq) << 19;
    return d | u32(delim_crc(d)) << 25;
}

// read the status register block, waiting out the firmware-held lock word
auto update_status(Status& status) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto deadline = time::now() + status_timeout_us;
    while(true) {
        co_ensure(read_multi(yaps.status_regs_addr, (u8*)status.regs, sizeof(status.regs)));
        if(status.regs[Status::Lock] == 0) {
            co_return true;
        }
        co_ensure(time::now() < deadline, "yaps status lock timeout");
        co_await coop::sleep_ms(1);
    }
}

// pop one frame from the from-chip stream into buf(yaps_max_rx); 0 = none pending
auto read_rx_frame(u8* const buf) -> noxx::Optional<u32> {
    constexpr auto error_value = noxx::nullopt;

    unwrap(delim, read_u32(yaps.ysl_addr));
    if(delim == 0) {
        return u32(0);
    }
    ensure(delim_crc(delim) == delim >> 25, "rx delimiter crc mismatch");
    const auto raw_size = delim & 0x3fff;
    const auto padding  = (delim >> 17) & 0x3;
    ensure(raw_size > yaps.reserved_page_size, "rx delimiter too small");
    const auto total = raw_size - yaps.reserved_page_size + padding;
    ensure(total % 4 == 0 && total >= skb_hdr_size && total <= yaps_max_rx, "bad rx frame size");
    ensure(read_multi(yaps.ysl_addr + 4, buf, total));
    ensure(buf[0] == skb_sync, "rx skb sync mismatch");
    return u32(total);
}

// handle a non-response frame; returns the command payload if this was one
auto dispatch_rx_frame(const u8* const buf, const u32 total) -> const u8* {
    const auto channel = buf[1];
    const auto len     = u32(get_u16(buf + 2));
    const auto offset  = u32(buf[4]);
    if(channel != skb_chan_cmd) {
        log<"halow: rx frame chan 0x{02x} len {} dropped\n">(channel, len);
        return nullptr;
    }
    if(skb_hdr_size + offset + cmd_hdr_size > total) {
        log<"halow: truncated command frame\n">();
        return nullptr;
    }
    const auto cmd   = buf + skb_hdr_size + offset;
    const auto flags = get_u16(cmd);
    if(flags & cmd_flag_event) {
        log<"halow: event 0x{04x} len {}\n">(get_u16(cmd + 2), get_u16(cmd + 4));
        return nullptr;
    }
    return cmd;
}
} // namespace

auto init_command(const YapsTable& new_yaps) -> void {
    yaps  = new_yaps;
    ready = true;
}

auto send_command(const u16 id, const noxx::Span<const u8> req, const noxx::Span<u8> resp) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    co_ensure(ready, "command channel not initialized");
    co_ensure(!busy, "command already in flight");
    struct BusyGuard {
        BusyGuard() { busy = true; }
        ~BusyGuard() { busy = false; }
    } busy_guard;

    // frame: [delimiter][skb header][command header][payload][pad to 4]
    const auto cmd_len   = cmd_hdr_size + req.size;
    const auto frame_len = (skb_hdr_size + cmd_len + 3) & ~u32(3);
    co_ensure(frame_len + yaps.reserved_page_size <= 0x3fff, "command too long");
    const auto frame = noxx::make_unique_array<u8>(4 + frame_len);
    co_ensure(frame);

    seq                = seq % 0xfff + 1;
    const auto host_id = u16(seq << 4);
    const auto p       = frame.get();
    put_u32(p, make_delim(frame_len + yaps.reserved_page_size, yaps_pool_cmd, true));
    p[4] = skb_sync;
    p[5] = skb_chan_cmd;
    put_u16(p + 6, cmd_len);
    const auto cmd = p + 4 + skb_hdr_size;
    put_u16(cmd + 0, cmd_flag_req);
    put_u16(cmd + 2, id);
    put_u16(cmd + 4, req.size);
    put_u16(cmd + 6, host_id);
    put_u16(cmd + 8, vif_id_invalid);
    noxx::memcpy(cmd + cmd_hdr_size, req.data, req.size);

    // wait for room in the chip's command queue and page pool
    const auto pages_needed = (frame_len + yaps.reserved_page_size + yaps_page_size - 1) / yaps_page_size + yaps_extra_pages;
    auto       status       = Status();
    auto       deadline     = time::now() + response_timeout_us;
    while(true) {
        co_ensure(co_await update_status(status));
        if(status.regs[Status::TcCmdPkts] < yaps.tc_cmd_q_size &&
           pages_needed <= status.regs[Status::TcCmdPoolPages]) {
            break;
        }
        co_ensure(time::now() < deadline, "yaps command queue full");
        co_await coop::sleep_ms(5);
    }

    co_ensure(write_multi(yaps.yds_addr, p, 4 + frame_len));

    // await the matching response, draining unrelated frames meanwhile
    const auto rx = noxx::make_unique_array<u8>(yaps_max_rx);
    co_ensure(rx);
    deadline = time::now() + response_timeout_us;
    while(true) {
        co_unwrap(total, read_rx_frame(rx.get()));
        if(total == 0) {
            co_ensure(time::now() < deadline, "command response timeout");
            co_await coop::sleep_ms(2);
            continue;
        }
        const auto rcmd = dispatch_rx_frame(rx.get(), total);
        if(rcmd == nullptr) {
            continue;
        }
        const auto flags = get_u16(rcmd);
        const auto rid   = get_u16(rcmd + 2);
        const auto rlen  = u32(get_u16(rcmd + 4));
        const auto rhid  = get_u16(rcmd + 6);
        if(!(flags & cmd_flag_resp) || rid != id || (rhid & host_id_seq_mask) != host_id) {
            log<"halow: stray response 0x{04x}:0x{04x}\n">(rid, rhid);
            continue;
        }
        co_ensure(rlen >= 4 && skb_hdr_size + cmd_hdr_size + rlen <= total, "malformed response");
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

auto poll_rx() -> coop::Async<noxx::Optional<bool>> {
    constexpr auto error_value = noxx::nullopt;

    co_ensure(ready, "command channel not initialized");
    const auto rx = noxx::make_unique_array<u8>(yaps_max_rx);
    co_ensure(rx);
    co_unwrap(total, read_rx_frame(rx.get()));
    if(total == 0) {
        co_return false;
    }
    if(const auto cmd = dispatch_rx_frame(rx.get(), total); cmd != nullptr) {
        log<"halow: unsolicited response 0x{04x}\n">(get_u16(cmd + 2));
    }
    co_return true;
}
} // namespace halow
