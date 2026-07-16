// fixed-size packet buffers from a static pool
#pragma once
#include <noxx/int.hpp>
#include <noxx/unique-ptr.hpp>

namespace net {
// max yaps rx frame (1628 bytes) + ethernet mtu frame + driver headers
constexpr auto packet_capacity = usize(1664);

struct Packet {
    Packet* next; // intrusive freelist / queue link
    u16     head; // offset of the first valid byte in buf
    u16     len;  // valid bytes from head
    u8      buf[packet_capacity];

    auto data() -> u8* {
        return buf + head;
    }

    auto data() const -> const u8* {
        return buf + head;
    }

    auto headroom() const -> usize {
        return head;
    }

    auto tailroom() const -> usize {
        return packet_capacity - head - len;
    }

    // grow the front by count bytes; returns the new data start, null if no headroom
    auto prepend(usize count) -> u8*;
    // grow the back by count bytes; returns the appended region, null if no tailroom
    auto append(usize count) -> u8*;
    // drop count bytes from the front
    auto consume(usize count) -> bool;
};

auto packet_pool_init() -> void;
auto packet_pool_avail() -> usize;
auto packet_alloc(usize headroom = 0) -> Packet*;
auto packet_free(Packet* packet) -> void;

struct PacketDeleter {
    static auto operator()(Packet* packet) -> void {
        packet_free(packet);
    }
};

using AutoPacket = noxx::UniquePtr<Packet, PacketDeleter>;
} // namespace net
