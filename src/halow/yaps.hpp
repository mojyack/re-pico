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

// rx_status flags (ref skb_header.h enum morse_rx_status_flags)
struct RxFlag {
    enum : u32 {
        Error       = 1 << 0,
        Decrypted   = 1 << 1,
        FcsIncluded = 1 << 2,
        Eof         = 1 << 3,
        Ampdu       = 1 << 4,
        Ndp         = 1 << 7,
        Uplink      = 1 << 8,
        CrcError    = 1 << 14,
    };
};

// tx_info flags (ref skb_header.h enum morse_tx_status_and_conf_flags)
struct TxFlag {
    enum : u32 {
        NoAck           = 1u << 0,
        HwEncrypt       = 1u << 3,
        ImmediateReport = 1u << 31,
    };
};

// vif id rides in tx_info flags bits [11:4]
inline auto tx_flag_vif(const u16 vif) -> u32 {
    return u32(vif & 0xff) << 4;
}

// morse rate codes (ref morse_rate_code.h): preamble[3:0] mcs[7:4] nss[10:8] bw[13:11]
struct RateCode {
    enum : u32 {
        Mcs0Bw1Mhz = 2,            // 1MHz preamble
        Mcs0Bw2Mhz = 1 << 11 | 1,  // 2MHz bw, s1g short preamble
    };
};

// tx_info fields written into the skb header (ref struct morse_skb_tx_info);
// only the first rate table entry is exposed
struct TxInfo {
    u32 flags    = 0; // TxFlag bits | tx_flag_vif()
    u32 pkt_id   = 0; // echoed in the tx status report
    u8  tid      = 0;
    u32 rate     = 0; // RateCode for rates[0], 0 = leave rate control to fw
    u8  attempts = 0; // tx attempts at rate
};

// decoded skb header of a received frame
struct SkbHeader {
    u8  channel;
    u16 len;    // payload bytes, after the header and offset pad
    u8  offset; // pad bytes between header and payload
    // rx_status fields, meaningful for from-air channels
    u32 rx_flags; // RxFlag bits
    u16 rssi;
    u16 freq_100khz;
};

// remember the yaps stream layout, must be called once after parse_host_table
auto init_yaps(const YapsTable& table) -> void;

// read the status block, waiting out the firmware-held lock word
auto read_status(YapsStatus& status) -> coop::Async<bool>;

// send the packet payload as one frame on channel, waiting for chip queue
// space; the packet needs tx_headroom of headroom and is not freed.
// info fills the skb header tx_info fields, null leaves them zero
auto yaps_tx(u8 channel, net::Packet& packet, const TxInfo* info = nullptr) -> coop::Async<bool>;

// pop one pending from-chip frame; the packet data starts at the skb header.
// returns nullptr if nothing is pending
auto yaps_rx() -> noxx::Optional<net::AutoPacket>;

// validate and decode the skb header at the start of an rx frame
auto parse_skb_header(const net::Packet& packet) -> noxx::Optional<SkbHeader>;

// start of the frame payload described by a decoded skb header
inline auto packet_frame(const net::Packet& packet, const SkbHeader& hdr) -> const u8* {
    return packet.data() + skb_hdr_size + hdr.offset;
}

// backlog of received frames nobody was waiting for, bounded; push frees on overflow
auto push_rx_backlog(net::Packet* packet) -> void;
auto pop_rx_backlog() -> net::Packet*;
} // namespace halow
