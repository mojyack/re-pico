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
        SubProbeReq  = 4 << 4,
        SubProbeResp = 5 << 4,
        SubAuth      = 11 << 4,
        SubDeauth    = 12 << 4,

        ProbeReq  = TypeMgmt | SubProbeReq,
        ProbeResp = TypeMgmt | SubProbeResp,

        VerTypeSubMask = 0x00ff, // frame identity, ignoring the flag bits
    };
};

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

// information element ids
struct Ie {
    enum : u8 {
        Ssid            = 0,
        ShortBcnInt     = 214,
        S1gCapabilities = 217,
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
} // namespace halow::dot11
