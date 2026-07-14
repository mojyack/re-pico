#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>

#include "host-table.hpp"
#include "skb.hpp"

// yaps stream datapath, the transport for every host<->firmware frame
// (see docs/yaps.md; ref morse_driver/mm8108/yaps-hw.c)
namespace halow {
constexpr auto tx_headroom = usize(4 + sizeof(SkbHeader)); // delimiter + skb header, required by yaps_tx

// yaps status register block, read as one chunk
// (ref yaps-hw.c struct morse_yaps_status_registers)
struct YapsStatus {
    enum : u32 {
        TcTxPoolPages = 0,
        TcCmdPoolPages,
        TcBeaconPoolPages,
        TcMgmtPoolPages,
        FcRxPoolPages,
        FcRespPoolPages,
        FcTxStsPoolPages,
        FcAuxPoolPages,
        TcTxPkts,
        TcCmdPkts,
        TcBeaconPkts,
        TcMgmtPkts,
        FcPkts,
        FcDonePkts,
        FcRxBytes,
        TcCrcFail,
        YslStatus,
        Lock,
        Count,
    };
    u32 regs[Count];
};

// remember the yaps stream layout, must be called once after parse_host_table
auto init_yaps(const YapsTable& table) -> void;

// read the status block, waiting out the firmware-held lock word
auto read_status(YapsStatus& status) -> coop::Async<bool>;

// send the packet payload as one frame on channel, waiting for chip queue
// space; the packet needs tx_headroom of headroom and is not freed.
// info fills the skb header tx_info fields, null leaves them zero
auto yaps_tx(u8 channel, net::Packet& packet, const SkbHeader::TxInfo* info = nullptr) -> coop::Async<bool>;

// pop one pending from-chip frame; the packet data starts at the skb header.
// returns nullptr if nothing is pending
auto yaps_rx() -> noxx::Optional<net::AutoPacket>;

// validate the skb header at the start of an rx frame; points into the packet,
// null if malformed
auto parse_skb_header(const net::Packet& packet) -> const SkbHeader*;

// start of the frame payload described by an skb header
inline auto packet_frame(const net::Packet& packet, const SkbHeader& hdr) -> const u8* {
    return packet.data() + sizeof(SkbHeader) + hdr.offset;
}

// backlog of received frames nobody was waiting for, bounded; push frees on overflow
auto push_rx_backlog(net::Packet* packet) -> void;
auto pop_rx_backlog() -> net::Packet*;
} // namespace halow
