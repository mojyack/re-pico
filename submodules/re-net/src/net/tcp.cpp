#include <coop/platform.hpp>
#include <coop/select.hpp>
#include <coop/timer.hpp>
#include <noxx/algorithm.hpp>
#include <noxx/endian.hpp>

#include "ethernet.hpp"
#include "stack.hpp"
#include "tcp.hpp"

#include <noxx/assert.hpp>

namespace net::tcp {
namespace {
// serial number arithmetic over the 32-bit sequence space
auto seq_lt(const u32 a, const u32 b) -> bool {
    return i32(a - b) < 0;
}

auto seq_le(const u32 a, const u32 b) -> bool {
    return i32(a - b) <= 0;
}

auto bind(Stack& stack, Conn& conn) -> bool {
    constexpr auto error_value = false;

    auto free = (Conn**)nullptr;
    for(auto& slot : stack.tcp.conns.data) {
        if(slot == nullptr) {
            free = free != nullptr ? free : &slot;
            continue;
        }
        ensure(slot->local_port != conn.local_port, "port in use");
    }
    ensure(free != nullptr, "connection table full");
    *free = &conn;
    return true;
}

auto unbind(Stack& stack, Conn& conn) -> void {
    for(auto& slot : stack.tcp.conns.data) {
        if(slot == &conn) {
            slot = nullptr;
        }
    }
}

// clear per-connection state for a (re)open; a conn object is reusable
auto reinit(Conn& conn) -> void {
    conn.snd_una       = 0;
    conn.snd_nxt       = 0;
    conn.snd_wnd       = 0;
    conn.snd_mss       = default_mss;
    conn.retries       = 0;
    conn.rcv_nxt       = 0;
    conn.rx_head       = 0;
    conn.rx_len        = 0;
    conn.rx_fin        = false;
    conn.rx_wnd_closed = false;
    conn.tx_event      = coop::SingleEvent();
    conn.rx_event      = coop::SingleEvent();
    conn.expire_ms     = 0;
}

// drop the connection and wake both directions
auto reset(Stack& stack, Conn& conn) -> void {
    unbind(stack, conn);
    conn.state = State::Closed;
    conn.tx_event.notify();
    conn.rx_event.notify();
}

// enter time-wait; tick() reaps the slot after the linger
auto time_wait(Stack& stack, Conn& conn) -> void {
    conn.state     = State::TimeWait;
    conn.expire_ms = stack.now_ms + time_wait_ms;
    conn.tx_event.notify();
    conn.rx_event.notify();
}

auto rx_free(const Conn& conn) -> usize {
    return rx_buffer_size - conn.rx_len;
}

// append to the receive ring; returns the bytes that fit
auto rx_push(Conn& conn, const noxx::Span<const u8> data) -> usize {
    const auto count = noxx::min(data.size(), rx_free(conn));
    for(auto i = usize(0); i < count; i += 1) {
        conn.rx_buf[(conn.rx_head + conn.rx_len + i) % rx_buffer_size] = data[i];
    }
    conn.rx_len += count;
    return count;
}

// pop from the receive ring into out; returns the bytes copied
auto rx_pop(Conn& conn, const noxx::Span<u8> out) -> usize {
    const auto count = noxx::min(out.size(), conn.rx_len);
    for(auto i = usize(0); i < count; i += 1) {
        out[i] = conn.rx_buf[(conn.rx_head + i) % rx_buffer_size];
    }
    conn.rx_head = (conn.rx_head + count) % rx_buffer_size;
    conn.rx_len -= count;
    return count;
}

// build and send one segment; the ack field always carries rcv_nxt when the
// Ack flag is set, and a syn carries our mss option
auto send_segment(Stack& stack, Conn& conn, const u8 flags, const u32 seq, const noxx::Span<const u8> data) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto opt_len  = (flags & Flags::Syn) != 0 ? usize(4) : usize(0);
    const auto headroom = sizeof(EthernetHeader) + sizeof(ipv4::Header);
    auto       packet   = AutoPacket(packet_alloc(headroom));
    co_ensure(packet.get() != nullptr);
    const auto raw = packet->append(sizeof(Header) + opt_len + data.size());
    co_ensure(raw != nullptr);

    const auto wnd = u16(noxx::min(rx_free(conn), usize(0xffff)));
    if(wnd == 0) {
        conn.rx_wnd_closed = true; // recv() reopens with a window update
    }
    auto& header    = *(Header*)raw;
    header          = Header{};
    header.src_port = noxx::byteswap(conn.local_port);
    header.dst_port = noxx::byteswap(conn.remote_port);
    header.seq      = noxx::byteswap(seq);
    header.ack      = (flags & Flags::Ack) != 0 ? noxx::byteswap(conn.rcv_nxt) : 0;
    header.data_off = u8((sizeof(Header) + opt_len) / 4 << 4);
    header.flags    = flags;
    header.window   = noxx::byteswap(wnd);
    if(opt_len != 0) {
        raw[sizeof(Header) + 0] = Option::Mss;
        raw[sizeof(Header) + 1] = 4;
        raw[sizeof(Header) + 2] = u8(rx_mss >> 8);
        raw[sizeof(Header) + 3] = u8(rx_mss & 0xff);
    }
    noxx::memcpy(raw + sizeof(Header) + opt_len, data.data, data.size());
    header.checksum = noxx::byteswap(checksum(stack.addr, conn.remote_addr, {raw, packet->len}));

    co_return co_await ipv4::output(stack, conn.remote_addr, ipv4::Proto::Tcp, noxx::move(packet));
}

// rfc 793 reset generation for a segment that matches no connection; offender
// fields are still big-endian
auto send_rst(Stack& stack, const IPv4Addr dst, const Header& offender, const usize data_len) -> coop::Async<void> {
    if((offender.flags & Flags::Rst) != 0) {
        co_return; // never reset a reset
    }
    const auto headroom = sizeof(EthernetHeader) + sizeof(ipv4::Header);
    auto       packet   = AutoPacket(packet_alloc(headroom));
    if(packet.get() == nullptr) {
        co_return;
    }
    const auto raw = packet->append(sizeof(Header));
    if(raw == nullptr) {
        co_return;
    }
    auto& header    = *(Header*)raw;
    header          = Header{};
    header.src_port = offender.dst_port;
    header.dst_port = offender.src_port;
    header.data_off = u8(sizeof(Header) / 4 << 4);
    if((offender.flags & Flags::Ack) != 0) {
        header.seq   = offender.ack;
        header.flags = Flags::Rst;
    } else {
        const auto ctrl = usize((offender.flags & Flags::Syn) != 0) + usize((offender.flags & Flags::Fin) != 0);
        header.ack      = noxx::byteswap(u32(noxx::byteswap(offender.seq) + data_len + ctrl));
        header.flags    = Flags::Rst | Flags::Ack;
    }
    header.checksum = noxx::byteswap(checksum(stack.addr, dst, {raw, packet->len}));
    co_await ipv4::output(stack, dst, ipv4::Proto::Tcp, noxx::move(packet));
}

// extract the mss option from a syn's option block; 0 if absent
auto parse_mss(const noxx::Span<const u8> options) -> u16 {
    constexpr auto error_value = 0;

    auto i = usize(0);
    while(i < options.size()) {
        const auto kind = options[i];
        if(kind == Option::End) {
            break;
        }
        if(kind == Option::Nop) {
            i += 1;
            continue;
        }
        ensure(i + 1 < options.size());
        const auto len = usize(options[i + 1]);
        ensure(len >= 2 && i + len <= options.size());
        if(kind == Option::Mss && len == 4) {
            return u16(options[i + 2]) << 8 | options[i + 3];
        }
        i += len;
    }
    return 0;
}

auto negotiate_mss(const noxx::Span<const u8> options) -> u16 {
    const auto mss = parse_mss(options);
    return mss != 0 ? noxx::min(mss, rx_mss) : default_mss;
}

auto wait_event(coop::SingleEvent& event) -> coop::Async<void> {
    co_await event;
}

auto sleep_task(const u64 ms) -> coop::Async<void> {
    co_await coop::sleep_ms(ms);
}

auto next_ephemeral_port() -> u16 {
    static auto counter = u16(0xc000);
    counter             = counter >= 0xfff0 ? u16(0xc000) : u16(counter + 1);
    return counter;
}
} // namespace

auto checksum(const IPv4Addr src, const IPv4Addr dst, const noxx::Span<const u8> segment) -> u16 {
    return ipv4::l4_checksum(src, dst, ipv4::Proto::Tcp, segment);
}

auto input(Stack& stack, const IPv4Addr src, const IPv4Addr dst, AutoPacket packet) -> coop::Async<void> {
    if(packet->len < sizeof(Header)) {
        co_return;
    }
    const auto& header = *(const Header*)packet->data();
    const auto  off    = usize(header.data_off >> 4) * 4;
    if(off < sizeof(Header) || packet->len < off) {
        co_return;
    }
    if(checksum(src, dst, {packet->data(), packet->len}) != 0) {
        co_return; // corrupt
    }
    const auto seq      = noxx::byteswap(header.seq);
    const auto ack      = noxx::byteswap(header.ack);
    const auto wnd      = noxx::byteswap(header.window);
    const auto flags    = header.flags;
    const auto src_port = noxx::byteswap(header.src_port);
    const auto dst_port = noxx::byteswap(header.dst_port);
    const auto options  = noxx::Span<const u8>{packet->data() + sizeof(Header), off - sizeof(Header)};
    const auto data     = noxx::Span<const u8>{packet->data() + off, packet->len - off};

    // demux: an exact four-tuple match first, then a listener on the port
    auto conn     = (Conn*)nullptr;
    auto listener = (Conn*)nullptr;
    for(auto& slot : stack.tcp.conns.data) {
        if(slot == nullptr || slot->local_port != dst_port) {
            continue;
        }
        if(slot->state == State::Listen) {
            listener = listener != nullptr ? listener : slot;
            continue;
        }
        if(slot->remote_port == src_port && slot->remote_addr == src) {
            conn = slot;
            break;
        }
    }
    conn = conn != nullptr ? conn : listener;
    if(conn == nullptr) {
        co_await send_rst(stack, src, header, data.size());
        co_return;
    }

    switch(conn->state) {
    case State::Listen: {
        if((flags & Flags::Rst) != 0) {
            co_return;
        }
        if((flags & Flags::Ack) != 0) {
            co_await send_rst(stack, src, header, data.size());
            co_return;
        }
        if((flags & Flags::Syn) == 0) {
            co_return;
        }
        conn->remote_addr = src;
        conn->remote_port = src_port;
        conn->rcv_nxt     = seq + 1;
        conn->snd_mss     = negotiate_mss(options);
        conn->snd_wnd     = wnd;
        const auto iss    = u32(coop::now_us());
        conn->snd_una     = iss;
        conn->snd_nxt     = iss + 1;
        conn->state       = State::SynReceived;
        co_await send_segment(stack, *conn, Flags::Syn | Flags::Ack, iss, {});
        co_return;
    }
    case State::SynSent: {
        const auto ack_ok = seq_lt(conn->snd_una, ack) && seq_le(ack, conn->snd_nxt);
        if((flags & Flags::Ack) != 0 && !ack_ok) {
            co_await send_rst(stack, src, header, data.size());
            co_return;
        }
        if((flags & Flags::Rst) != 0) {
            if((flags & Flags::Ack) != 0) {
                reset(stack, *conn); // connection refused
            }
            co_return;
        }
        if((flags & Flags::Syn) == 0 || (flags & Flags::Ack) == 0) {
            co_return; // simultaneous open unsupported
        }
        conn->rcv_nxt = seq + 1;
        conn->snd_mss = negotiate_mss(options);
        conn->snd_una = ack;
        conn->snd_wnd = wnd;
        conn->state   = State::Established;
        co_await send_segment(stack, *conn, Flags::Ack, conn->snd_nxt, {});
        conn->tx_event.notify();
        co_return;
    }
    default:
        break; // synchronized states, handled below
    }

    // rst: accept only at the expected sequence (a strict check; the peer retransmits an unanswered rst anyway)
    if((flags & Flags::Rst) != 0) {
        if(seq == conn->rcv_nxt || conn->state == State::SynReceived) {
            reset(stack, *conn);
        }
        co_return;
    }
    // a retransmitted syn means our syn-ack was lost: resend it
    if(conn->state == State::SynReceived && (flags & Flags::Syn) != 0 && seq + 1 == conn->rcv_nxt) {
        co_await send_segment(stack, *conn, Flags::Syn | Flags::Ack, conn->snd_una, {});
        co_return;
    }
    // any other in-window syn is fatal
    if((flags & Flags::Syn) != 0 && seq_le(conn->rcv_nxt, seq)) {
        reset(stack, *conn);
        co_return;
    }
    if((flags & Flags::Ack) == 0) {
        co_return;
    }

    // ack processing
    if(conn->state == State::SynReceived) {
        if(!(seq_le(conn->snd_una, ack) && seq_le(ack, conn->snd_nxt))) {
            co_await send_rst(stack, src, header, data.size());
            co_return;
        }
        conn->state = State::Established;
    }
    if(seq_le(ack, conn->snd_nxt) && seq_le(conn->snd_una, ack)) {
        conn->snd_una = ack;
        conn->snd_wnd = wnd;
        if(conn->snd_una == conn->snd_nxt) {
            // everything including any in-flight fin is acknowledged
            switch(conn->state) {
            case State::FinWait1:
                conn->state = State::FinWait2;
                break;
            case State::Closing:
                time_wait(stack, *conn);
                break;
            case State::LastAck:
                unbind(stack, *conn);
                conn->state = State::Closed;
                break;
            default:
                break;
            }
        }
    }
    conn->tx_event.notify();

    // in-order data; anything past rcv_nxt is dropped and elicits a dup-ack
    const auto prev_rcv = conn->rcv_nxt;
    if(data.size() != 0 && (conn->state == State::Established || conn->state == State::FinWait1 || conn->state == State::FinWait2)) {
        auto body = data;
        auto bseq = seq;
        if(seq_lt(bseq, conn->rcv_nxt)) { // trim an already-received prefix
            const auto skip = usize(conn->rcv_nxt - bseq);
            body            = skip < body.size() ? body.subspan(skip) : noxx::Span<const u8>{};
            bseq            = conn->rcv_nxt;
        }
        if(body.size() != 0 && bseq == conn->rcv_nxt) {
            const auto accepted = rx_push(*conn, body);
            conn->rcv_nxt += accepted; // a full ring truncates; the peer retransmits the rest
            if(accepted != 0) {
                conn->rx_event.notify();
            }
        }
    }
    // fin, once every preceding byte has been received
    if((flags & Flags::Fin) != 0 && seq + data.size() == conn->rcv_nxt && !conn->rx_fin) {
        conn->rcv_nxt += 1;
        conn->rx_fin = true;
        switch(conn->state) {
        case State::Established:
            conn->state = State::CloseWait;
            break;
        case State::FinWait1:
            conn->state = State::Closing;
            break;
        case State::FinWait2:
            time_wait(stack, *conn);
            break;
        default:
            break;
        }
        conn->rx_event.notify();
        conn->tx_event.notify();
    }
    // ack progress, and re-ack duplicate or out-of-order data and fins
    if(conn->rcv_nxt != prev_rcv || data.size() != 0 || (flags & Flags::Fin) != 0) {
        co_await send_segment(stack, *conn, Flags::Ack, conn->snd_nxt, {});
    }
}

auto tick(Stack& stack, const u64 now_ms) -> void {
    for(auto& slot : stack.tcp.conns.data) {
        if(slot != nullptr && slot->state == State::TimeWait && now_ms >= slot->expire_ms) {
            slot->state = State::Closed;
            slot        = nullptr;
        }
    }
}

auto Conn::listen(Stack& stack, const u16 port) -> bool {
    constexpr auto error_value = false;

    ensure(state == State::Closed, "connection in use");
    reinit(*this);
    local_port  = port;
    remote_port = 0;
    remote_addr = {};
    ensure(bind(stack, *this));
    state = State::Listen;
    return true;
}

auto Conn::accept(Stack& /*stack*/) -> coop::Async<bool> {
    while(state == State::Listen || state == State::SynReceived) {
        tx_event = coop::SingleEvent();
        co_await tx_event;
    }
    co_return state != State::Closed;
}

auto Conn::connect(Stack& stack, const IPv4Addr addr, const u16 port) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(state == State::Closed, "connection in use");
    reinit(*this);
    remote_addr = addr;
    remote_port = port;
    local_port  = next_ephemeral_port();
    co_ensure(bind(stack, *this));

    const auto iss = u32(coop::now_us());
    snd_una        = iss;
    snd_nxt        = iss + 1;
    state          = State::SynSent;
    for(auto i = u32(0); i <= max_retx; i += 1) {
        tx_event = coop::SingleEvent();
        if(!co_await send_segment(stack, *this, Flags::Syn, iss, {})) {
            break;
        }
        co_await coop::select(wait_event(tx_event), sleep_task(rto_ms));
        if(state == State::Established) {
            co_return true;
        }
        if(state == State::Closed) {
            co_return false; // refused
        }
    }
    unbind(stack, *this);
    state = State::Closed;
    co_return false; // no response
}

auto Conn::send(Stack& stack, const noxx::Span<const u8> data) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(state == State::Established || state == State::CloseWait);
    if(data.size() == 0) {
        co_return true;
    }
    // data[i] occupies sequence base + i; resolve once the peer acked the lot
    const auto base = snd_nxt;
    const auto end  = u32(base + data.size());
    retries         = 0;
    while(seq_lt(snd_una, end)) {
        co_ensure(state == State::Established || state == State::CloseWait, "connection reset");
        tx_event = coop::SingleEvent();
        // fill the peer's window from snd_nxt in mss-sized segments
        while(seq_lt(snd_nxt, end)) {
            const auto in_flight = usize(snd_nxt - snd_una);
            auto       avail     = snd_wnd > in_flight ? snd_wnd - in_flight : usize(0);
            if(avail == 0) {
                if(in_flight != 0) {
                    break;
                }
                avail = 1; // zero window: probe with one byte
            }
            const auto count = noxx::min(noxx::min(avail, usize(snd_mss)), usize(end - snd_nxt));
            const auto last  = snd_nxt + count == end;
            const auto chunk = data.subspan(snd_nxt - base, count);
            co_ensure(co_await send_segment(stack, *this, u8(Flags::Ack | (last ? Flags::Psh : 0)), snd_nxt, chunk));
            snd_nxt = u32(snd_nxt + count);
        }
        if(co_await coop::select(wait_event(tx_event), sleep_task(rto_ms)) == 1) {
            // timeout: go-back-n from the oldest unacked byte. a zero window
            // is the peer's flow control, not a loss; do not count it
            if(snd_wnd != 0) {
                retries += 1;
                co_ensure(retries <= max_retx, "retransmission limit");
            }
            snd_nxt = snd_una;
        } else {
            retries = 0;
        }
    }
    co_return true;
}

auto Conn::recv(Stack& stack, const noxx::Span<u8> buffer) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    while(true) {
        if(rx_len != 0) {
            const auto count = rx_pop(*this, buffer);
            if(rx_wnd_closed) { // reopen a closed window now that there is room
                rx_wnd_closed = false;
                co_await send_segment(stack, *this, Flags::Ack, snd_nxt, {});
            }
            co_return usize(count);
        }
        if(rx_fin) {
            co_return usize(0); // orderly eof
        }
        co_ensure(state != State::Closed && state != State::Listen, "connection reset");
        rx_event = coop::SingleEvent();
        co_await rx_event;
    }
}

auto Conn::close(Stack& stack) -> coop::Async<void> {
    switch(state) {
    case State::Established:
        state = State::FinWait1;
        break;
    case State::CloseWait:
        state = State::LastAck;
        break;
    case State::Listen:
    case State::SynSent:
        unbind(stack, *this);
        state = State::Closed;
        co_return;
    default:
        co_return; // closed or already closing
    }
    // send the fin (it consumes one sequence number) and retransmit until acked
    const auto fin_seq = snd_nxt;
    snd_nxt            = u32(snd_nxt + 1);
    retries            = 0;
    while(seq_lt(snd_una, snd_nxt) && state != State::Closed) {
        tx_event = coop::SingleEvent();
        if(!co_await send_segment(stack, *this, Flags::Fin | Flags::Ack, fin_seq, {})) {
            break;
        }
        if(co_await coop::select(wait_event(tx_event), sleep_task(rto_ms)) == 1) {
            retries += 1;
            if(retries > max_retx) {
                break;
            }
        }
    }
    if(seq_lt(snd_una, snd_nxt) && state != State::Closed) {
        co_await abort(stack); // fin never acknowledged
        co_return;
    }
    // await the peer's fin for a bounded time; input() moves us to time-wait
    if(state == State::FinWait2) {
        tx_event = coop::SingleEvent();
        co_await coop::select(wait_event(tx_event), sleep_task(fin_wait_ms));
        if(state == State::FinWait2) {
            co_await abort(stack); // the peer never closed; give up
        }
    }
}

auto Conn::abort(Stack& stack) -> coop::Async<void> {
    switch(state) {
    case State::Closed:
    case State::Listen:
    case State::SynSent:
        break;
    default:
        co_await send_segment(stack, *this, Flags::Rst | Flags::Ack, snd_nxt, {});
        break;
    }
    reset(stack, *this);
}
} // namespace net::tcp
