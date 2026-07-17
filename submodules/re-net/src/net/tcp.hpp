// transmission control protocol (rfc 793), minimal single-task-per-direction
// implementation: in-order receive only (out-of-order segments are dropped and
// dup-acked), go-back-n retransmission driven from the sending coroutine
#pragma once
#include <coop/promise.hpp>
#include <coop/single-event.hpp>
#include <net/ip.hpp>
#include <net/packet.hpp>
#include <noxx/array.hpp>
#include <noxx/int.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

namespace net {
struct Stack;
} // namespace net

namespace net::tcp {
// tuning
constexpr auto table_size     = usize(4);    // concurrent connections
constexpr auto rx_buffer_size = usize(2048); // per-connection receive ring
constexpr auto rx_mss         = u16(1460);   // mss we advertise in a syn
constexpr auto default_mss    = u16(536);    // peer mss when it sends no option
constexpr auto rto_ms         = u64(1'000);  // fixed retransmission timeout
constexpr auto max_retx       = u32(5);      // give up after this many timeouts
constexpr auto time_wait_ms   = u64(10'000); // time-wait linger (well under 2msl; the table is tiny)
constexpr auto fin_wait_ms    = u64(10'000); // close() wait for the peer's fin

// tcp header without options; multi-byte scalars are big-endian
struct Header {
    u16 src_port;
    u16 dst_port;
    u32 seq;
    u32 ack;
    u8  data_off; // header length in words, in the upper 4 bits
    u8  flags;
    u16 window;
    u16 checksum;
    u16 urgent;
} __attribute__((packed));
static_assert(sizeof(Header) == 20);

struct Flags {
    enum : u8 {
        Fin = 0x01,
        Syn = 0x02,
        Rst = 0x04,
        Psh = 0x08,
        Ack = 0x10,
        Urg = 0x20,
    };
};

struct Option {
    enum : u8 {
        End = 0,
        Nop = 1,
        Mss = 2,
    };
};

enum class State : u8 {
    Closed,
    Listen,      // passive open, awaiting a syn
    SynSent,     // active open, syn sent
    SynReceived, // syn-ack sent, awaiting the final ack
    Established,
    FinWait1,  // we closed first, fin sent
    FinWait2,  // our fin acked, awaiting the peer's fin
    Closing,   // simultaneous close, awaiting the ack of our fin
    CloseWait, // peer closed first, we may still send
    LastAck,   // fin sent after close-wait, awaiting its ack
    TimeWait,  // fully closed, absorbing stray segments until expiry
};

struct Conn {
    State    state       = State::Closed;
    u16      local_port  = 0;
    u16      remote_port = 0;
    IPv4Addr remote_addr = {};

    // send sequence state (rfc 793 names)
    u32 snd_una = 0; // oldest unacknowledged
    u32 snd_nxt = 0; // next to send
    u16 snd_wnd = 0; // peer's advertised window
    u16 snd_mss = default_mss;
    u32 retries = 0; // consecutive timeouts in the current operation

    // receive state: an in-order ring; anything else is dropped and dup-acked
    u32                             rcv_nxt       = 0;
    noxx::Array<u8, rx_buffer_size> rx_buf        = {};
    usize                           rx_head       = 0;
    usize                           rx_len        = 0;
    bool                            rx_fin        = false; // peer's fin consumed into the stream
    bool                            rx_wnd_closed = false; // we advertised a zero window

    // input() wakes these; one sender and one receiver task per connection
    coop::SingleEvent tx_event; // ack progress, window update or state change
    coop::SingleEvent rx_event; // data, fin or state change

    u64 expire_ms = 0; // time-wait deadline, reaped by tick()

    // passive open: bind and await a syn; accept() resolves once established
    auto listen(Stack& stack, u16 port) -> bool;
    auto accept(Stack& stack) -> coop::Async<bool>;
    // active open: three-way handshake toward addr:port, retransmitting the syn
    auto connect(Stack& stack, IPv4Addr addr, u16 port) -> coop::Async<bool>;
    // reliable send; resolves once every byte is acknowledged
    auto send(Stack& stack, noxx::Span<const u8> data) -> coop::Async<bool>;
    // await data; returns bytes copied, 0 at orderly eof, nullopt after a reset
    auto recv(Stack& stack, noxx::Span<u8> buffer) -> coop::Async<noxx::Optional<usize>>;
    // orderly close (fin handshake); falls back to a reset on timeout
    auto close(Stack& stack) -> coop::Async<void>;
    // immediate reset
    auto abort(Stack& stack) -> coop::Async<void>;
};

struct Table {
    noxx::Array<Conn*, table_size> conns = {};
};

// ones-complement checksum over the ipv4 pseudo-header and the tcp segment
// (host order return); a valid received segment sums to zero
auto checksum(IPv4Addr src, IPv4Addr dst, noxx::Span<const u8> segment) -> u16;

// handle a received tcp segment (ip header already consumed); dst is the ip
// destination, needed for the checksum pseudo-header
auto input(Stack& stack, IPv4Addr src, IPv4Addr dst, AutoPacket packet) -> coop::Async<void>;

// reap expired time-wait connections
auto tick(Stack& stack, u64 now_ms) -> void;
} // namespace net::tcp
