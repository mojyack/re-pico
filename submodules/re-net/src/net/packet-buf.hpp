#include <noxx/buf-reader.hpp>
#include <noxx/buf-writer.hpp>

#include "packet.hpp"

namespace net {
struct BufRWPacketBuffer {
    Packet* packet;

    auto read(const usize len) -> const u8* {
        const auto ptr = packet->data();
        return packet->consume(len) ? ptr : nullptr;
    }

    auto alloc(const usize len) -> u8* {
        return packet->append(len);
    }

    BufRWPacketBuffer() = default;

    BufRWPacketBuffer(Packet& packet)
        : packet(&packet) {
    }
};

using PacketReader = noxx::BufReader<BufRWPacketBuffer>;
using PacketWriter = noxx::BufWriter<BufRWPacketBuffer>;
} // namespace net
