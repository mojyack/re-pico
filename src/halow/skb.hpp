#pragma once
#include <noxx/int.hpp>

// morse skb header, prepended to every host<->firmware frame
// (ref morse_driver/skb_header.h)
namespace halow {
constexpr auto skb_sync            = u8(0xAA); // MORSE_SKB_HEADER_SYNC
constexpr auto skb_chip_owned_sync = u8(0xBB); // MORSE_SKB_HEADER_CHIP_OWNED_SYNC

// skb channels (enum morse_skb_channel)
struct SkbChan {
    enum : u8 {
        Data               = 0x00,
        NdpFrames          = 0x01,
        DataNoAck          = 0x02,
        Beacon             = 0x03,
        Mgmt               = 0x04,
        Wiphy              = 0x05,
        InternalCritBeacon = 0x80,
        Loopback           = 0xEE,
        Command            = 0xFE,
        TxStatus           = 0xFF,
    };
};

// tx_info / tx_status flags (enum morse_tx_status_and_conf_flags);
// multi-bit fields go through BF()/FB()
struct TxFlag {
    enum : u32 {
        NoAck             = 1u << 0,
        NoReport          = 1u << 1,
        CtlAmpdu          = 1u << 2,
        HwEncrypt         = 1u << 3,
        VifId             = 0xffu << 4,
        KeyIdx            = 0x7u << 12,
        PsFiltered        = 1u << 15,
        IgnoreTwt         = 1u << 16,
        PageInvalid       = 1u << 17,
        NoPsBuffer        = 1u << 18,
        DutyCycleCantSend = 1u << 19,
        HasPv1BpnInBody   = 1u << 21,
        SendAfterDtim     = 1u << 22,
        WasAggregated     = 1u << 23,
        FullmacReport     = 1u << 24,
        ImmediateReport   = 1u << 31,
    };
};

// rx_status flags (enum morse_rx_status_flags)
struct RxFlag {
    enum : u32 {
        Error       = 1u << 0,
        Decrypted   = 1u << 1,
        FcsIncluded = 1u << 2,
        Eof         = 1u << 3,
        Ampdu       = 1u << 4,
        Ndp         = 1u << 7,
        Uplink      = 1u << 8,
        Ri          = 0x3u << 9,
        NdpType     = 0x7u << 11,
        CrcError    = 1u << 14,
        VifId       = 0xffu << 17,
    };
};

// tx_info tid_params bits (TX_INFO_TID_PARAMS_*)
struct TidParams {
    enum : u8 {
        MaxReorderBuf  = 0x1f,
        AmpduEnabled   = 0x20,
        AmsduSupported = 0x40,
        UseLegacyBa    = 0x80,
    };
};

// tx_info mmss_params fields (TX_INFO_MMSS_PARAMS_*)
struct MmssParams {
    enum : u8 {
        Mmss       = 0x0f,
        MmssOffset = 0xf0,
    };
};

// tx_status ampdu_info fields (MORSE_TXSTS_AMPDU_INFO_*)
struct AmpduInfo {
    enum : u16 {
        Success = 0x001f,
        Len     = 0x03e0,
        Tag     = 0xfc00,
    };
};

constexpr auto skb_max_rates = usize(4); // MORSE_SKB_MAX_RATES

// ref struct morse_buff_skb_header
struct SkbHeader {
    struct RateInfo { // struct morse_skb_rate_info
        u32 rate_code;
        u8  count;
    } __attribute__((packed));

    struct TxInfo { // struct morse_skb_tx_info
        u32      flags; // TxFlag
        u32      pkt_id;
        u8       tid;
        u8       tid_params;  // TidParams
        u8       mmss_params; // MmssParams
        u8       padding[1];
        RateInfo rates[skb_max_rates];
    } __attribute__((packed));

    struct TxStatus { // struct morse_skb_tx_status
        u32      flags; // TxFlag
        u32      pkt_id;
        u8       tid;
        u8       channel;
        u16      ampdu_info; // AmpduInfo
        RateInfo rates[skb_max_rates];
    } __attribute__((packed));

    struct RxStatus { // struct morse_skb_rx_status
        u32 flags; // RxFlag
        u32 rate_code;
        u16 rssi;
        u16 freq_100khz;
        u8  bss_color;
        i8  noise_dbm;
        u8  padding[2];
        u64 rx_timestamp_us;
    } __attribute__((packed));

    u8  sync; // skb_sync
    u8  channel;
    u16 len;
    u8  offset;
    u8  checksum_lower;
    u16 checksum_upper;

    union {
        TxInfo   tx_info;
        TxStatus tx_status;
        RxStatus rx_status;
    };
} __attribute__((packed));
static_assert(sizeof(SkbHeader) == 40);
} // namespace halow
