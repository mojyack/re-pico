#pragma once
#include <noxx/int.hpp>

// 802.11 frame constants (ref dot11/dot11.h, dot11/dot11_frames.h)
namespace halow::dot11 {
constexpr auto mac_len = usize(6);
constexpr auto fcs_len = usize(4);

// frame control: version[1:0] | type[3:2] | subtype[7:4] | flags[15:8]
struct Fc {
    enum : u16 {
        TypeMgmt = 0b00 << 2,
        TypeCtrl = 0b01 << 2,
        TypeData = 0b10 << 2,
        TypeExt  = 0b11 << 2,

        // mgmt subtypes
        SubAssocReq  = 0 << 4,
        SubAssocResp = 1 << 4,
        SubProbeReq  = 4 << 4,
        SubProbeResp = 5 << 4,
        SubAuth      = 11 << 4,
        SubDeauth    = 12 << 4,

        // data subtypes
        SubQosData = 8 << 4,

        // ext subtypes
        SubS1gBeacon = 1 << 4,
        S1gBeacon    = TypeExt | SubS1gBeacon,

        ProbeReq   = TypeMgmt | SubProbeReq,
        ProbeResp  = TypeMgmt | SubProbeResp,
        AssocReq   = TypeMgmt | SubAssocReq,
        AssocResp  = TypeMgmt | SubAssocResp,
        Auth       = TypeMgmt | SubAuth,
        Deauth     = TypeMgmt | SubDeauth,
        QosData    = TypeData | SubQosData,

        // flag bits in the high byte
        ToDs      = 1 << 8,
        FromDs    = 1 << 9,
        Protected = 1 << 14,

        VerTypeSubMask = 0x00ff, // frame identity, ignoring the flag bits
    };
};

// authentication algorithms (DOT11_AUTH_ALG_*)
struct AuthAlg {
    enum : u16 {
        Open = 0,
        Sae  = 3,
    };
};

// capability information bits (DOT11_MASK_CAPINFO_*)
struct CapInfo {
    enum : u16 {
        Ess           = 1 << 0,
        Privacy       = 1 << 4,
        ShortPreamble = 1 << 5,
        Qos           = 1 << 9,
        ShortSlotTime = 1 << 10,
    };
};

constexpr auto status_success = u16(0);

// pv0 mgmt header byte offsets (struct dot11_hdr)
struct Hdr {
    enum : u32 {
        FrameControl = 0,
        Duration     = 2,
        Addr1        = 4,  // destination
        Addr2        = 10, // source
        Addr3        = 16, // bssid for mgmt frames
        SeqCtrl      = 22,
        Size         = 24,
    };
};

// probe response / beacon body offsets (struct dot11_probe_response)
struct ProbeResp {
    enum : u32 {
        Timestamp      = Hdr::Size,
        BeaconInterval = Hdr::Size + 8,
        Capability     = Hdr::Size + 10,
        Ies            = Hdr::Size + 12,
    };
};

// authentication frame body offsets (struct dot11_auth_hdr + dot11_auth_seq_status)
struct Auth {
    enum : u32 {
        Alg    = Hdr::Size,
        Seq    = Hdr::Size + 2,
        Status = Hdr::Size + 4,
        Size   = Hdr::Size + 6,
    };
};

// association request body offsets (struct dot11_assoc_req)
struct AssocReq {
    enum : u32 {
        Capability     = Hdr::Size,
        ListenInterval = Hdr::Size + 2,
        Ies            = Hdr::Size + 4,
    };
};

// association response body offsets (struct dot11_assoc_rsp, s1g format: no aid field)
struct AssocResp {
    enum : u32 {
        Capability = Hdr::Size,
        Status     = Hdr::Size + 2,
        Ies        = Hdr::Size + 4,
    };
};

// qos data frame offsets (struct dot11_data_hdr, 3-address + qos control)
struct QosData {
    enum : u32 {
        QosCtrl = Hdr::Size,
        Size    = Hdr::Size + 2,
    };
};

// information element ids
struct Ie {
    enum : u8 {
        Ssid            = 0,
        AidRequest      = 210,
        AidResponse     = 211,
        ShortBcnInt     = 214,
        S1gCapabilities = 217,
        VendorSpecific  = 221,
        S1gOperation    = 232,
    };
};
constexpr auto ie_hdr_size = usize(2); // element id + length

// s1g capabilities information bits (DOT11_S1G_CAP*, one enum per info byte)
struct S1gCap0 {
    enum : u8 {
        SuppWidth12Mhz     = 0 << 6,
        SuppWidth124Mhz    = 1 << 6,
        SuppWidth1248Mhz   = 2 << 6,
        SuppWidth124816Mhz = 3 << 6,
    };
};
struct S1gCap4 {
    enum : u8 {
        StaTypeSensor    = 1 << 6,
        StaTypeNonSensor = 2 << 6,
    };
};
struct S1gCap7 {
    enum : u8 {
        Dup1MhzSupport = 1 << 1,
    };
};

// s1g mcs map: 2-bit max-mcs per nss (DOT11_S1G_NSS_MAX_*)
struct S1gNssMaxMcs {
    enum : u8 {
        Mcs2 = 0,
        Mcs7 = 1,
        Mcs9 = 2,
        None = 3,
    };
};

// s1g operation ie body offsets (struct dot11_ie_s1g_operation, after the ie header)
struct S1gOp {
    enum : u32 {
        ChannelWidth   = 0, // S1gOpWidth bits
        OperatingClass = 1,
        PrimChanNum    = 2,
        OpChanNum      = 3, // channel center frequency, as an s1g channel number
        Size           = 6,
    };
};

// s1g operation channel width byte bits (DOT11_MASK_S1G_OP_CHAN_WIDTH_*)
struct S1gOpWidth {
    enum : u8 {
        PrimIs1Mhz  = 1 << 0, // set: 1MHz primary, clear: 2MHz primary
        OpWidthMask = 0xf << 1, // operating width minus one
        PrimLoc     = 1 << 5, // upper/lower 1MHz of the primary channel
    };
};

// llc/snap header for 802.11 data payloads (rfc 1042)
constexpr u8   llc_snap[]   = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};
constexpr auto llc_snap_len = usize(8); // 6 byte header + 2 byte ethertype
} // namespace halow::dot11
