#pragma once
#include <coop/generator.hpp>
#include <net/packet.hpp>
#include <noxx/optional.hpp>

#include "host-table.hpp"

// yaps stream datapath, the transport for every host<->firmware frame
// (see docs/yaps.md; ref morse_driver/mm8108/yaps-hw.c)
namespace halow {
// morse skb channels (ref morse_driver/skb_header.h)
struct SkbChan {
    enum : u8 {
        Data      = 0x00,
        NdpFrames = 0x01,
        DataNoAck = 0x02,
        Beacon    = 0x03,
        Mgmt      = 0x04,
        Loopback  = 0xEE,
        Command   = 0xFE,
        TxStatus  = 0xFF,
    };
};

constexpr auto skb_sync     = u8(0xAA);
constexpr auto skb_hdr_size = u32(40);                 // 8 byte header + 32 byte tx/rx info union
constexpr auto tx_headroom  = usize(4 + skb_hdr_size); // delimiter + skb header, required by yaps_tx

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

// decoded skb header of a received frame
struct SkbHeader {
    u8  channel;
    u16 len;    // payload bytes, after the header and offset pad
    u8  offset; // pad bytes between header and payload
    // rx_status fields, meaningful for from-air channels
    u16 rssi;
    u16 freq_100khz;
};

// remember the yaps stream layout, must be called once after parse_host_table
auto init_yaps(const YapsTable& table) -> void;

// read the status block, waiting out the firmware-held lock word
auto read_status(YapsStatus& status) -> coop::Async<bool>;

// send the packet payload as one frame on channel, waiting for chip queue
// space; the packet needs tx_headroom of headroom and is not freed.
// the skb header tx_info fields are left zero (no rate control hints)
auto yaps_tx(u8 channel, net::Packet& packet) -> coop::Async<bool>;

// pop one pending from-chip frame; the packet data starts at the skb header.
// returns nullptr if nothing is pending
auto yaps_rx() -> noxx::Optional<net::AutoPacket>;

// validate and decode the skb header at the start of an rx frame
auto parse_skb_header(const net::Packet& packet) -> noxx::Optional<SkbHeader>;

// backlog of received frames nobody was waiting for, bounded; push frees on overflow
auto push_rx_backlog(net::Packet* packet) -> void;
auto pop_rx_backlog() -> net::Packet*;
} // namespace halow
