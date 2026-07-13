#include <noxx/array.hpp>

#include "packet.hpp"

#include <noxx/assert.hpp>

namespace net {
namespace {
constexpr auto pool_size = usize(8);

auto pool      = noxx::Array<Packet, pool_size>();
auto freelist  = (Packet*)(nullptr);
auto available = usize(0);
} // namespace

auto Packet::prepend(const usize count) -> u8* {
    constexpr auto error_value = nullptr;

    ensure(headroom() >= count);
    head -= count;
    len += count;
    return data();
}

auto Packet::append(const usize count) -> u8* {
    constexpr auto error_value = nullptr;

    ensure(tailroom() >= count);
    const auto p = buf + head + len;
    len += count;
    return p;
}

auto Packet::consume(const usize count) -> bool {
    constexpr auto error_value = false;

    ensure(len >= count);
    head += count;
    len -= count;
    return true;
}

auto packet_pool_init() -> void {
    for(auto i = usize(0); i < pool_size; i += 1) {
        pool[i].next = freelist;
        freelist     = &pool[i];
    }
    available = pool_size;
}

auto packet_pool_avail() -> usize {
    return available;
}

auto packet_alloc(const usize headroom) -> Packet* {
    constexpr auto error_value = nullptr;

    ensure(headroom <= packet_capacity);
    ensure(freelist != nullptr, "packet pool exhausted");
    const auto packet = freelist;
    freelist          = packet->next;
    available -= 1;
    packet->next = nullptr;
    packet->head = headroom;
    packet->len  = 0;
    return packet;
}

auto packet_free(Packet* const packet) -> void {
    if(packet == nullptr) {
        return;
    }
    packet->next = freelist;
    freelist     = packet;
    available += 1;
}
} // namespace net
