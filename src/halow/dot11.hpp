#pragma once
#include <noxx/array.hpp>

// 802.11 frame constants (ref dot11/dot11.h, dot11/dot11_frames.h)
namespace halow::dot11 {
using MacAddr = noxx::Array<u8, 6>;

constexpr auto fcs_len = usize(4);

// CCMP-128: 8-byte header (pn + key id) after the mac header, 8-byte mic
// before the fcs. present on protected frames even after hardware decryption
constexpr auto ccmp_hdr_len = usize(8);
constexpr auto ccmp_mic_len = usize(8);

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
        SubDisassoc  = 10 << 4,
        SubAuth      = 11 << 4,
        SubDeauth    = 12 << 4,

        // data subtypes
        SubQosData = 8 << 4,
        SubQosNull = 12 << 4,

        // ext subtypes
        SubS1gBeacon = 1 << 4,
        S1gBeacon    = TypeExt | SubS1gBeacon,

        ProbeReq  = TypeMgmt | SubProbeReq,
        ProbeResp = TypeMgmt | SubProbeResp,
        AssocReq  = TypeMgmt | SubAssocReq,
        AssocResp = TypeMgmt | SubAssocResp,
        Auth      = TypeMgmt | SubAuth,
        Deauth    = TypeMgmt | SubDeauth,
        Disassoc  = TypeMgmt | SubDisassoc,
        QosData   = TypeData | SubQosData,
        QosNull   = TypeData | SubQosNull,

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

// pv0 mgmt header (struct dot11_hdr)
struct Header {
    u16     frame_control;
    u16     duration;
    MacAddr addr1;
    MacAddr addr2;
    MacAddr addr3;
    u16     sequence_control;
} __attribute__((packed));

static_assert(sizeof(Header) == 24);

// probe response / beacon (struct dot11_probe_response)
struct ProbeResponse {
    Header header;
    u8     timestamp[8];
    u16    beacon_interval;
    u16    capability_info;
    u8     ies[];
} __attribute__((packed));

// authentication frame body (struct dot11_auth_hdr + dot11_auth_seq_status)
struct Auth {
    Header header;
    u16    alg;
    u16    seq;
    u16    status_code;
} __attribute__((packed));

// association request body (struct dot11_assoc_req)
struct AssocReq {
    Header header;
    u16    capability;
    u16    listen_interval;
    u8     ies[];
} __attribute__((packed));

// association response body (struct dot11_assoc_rsp, s1g format: no aid field)
struct AssocResp {
    Header header;
    u16    capability;
    u16    status_code;
    u8     ies[];
} __attribute__((packed));

// qos data frame header (struct dot11_data_hdr, 3-address + qos control)
struct QosData {
    Header header;
    u16    qos_control;
} __attribute__((packed));

// deauthentication frame body (struct dot11_deauth)
struct Deauth {
    Header header;
    u16    reason_code; // Reason
} __attribute__((packed));

// reason codes (DOT11_REASON_*)
struct Reason {
    enum : u16 {
        DeauthLeaving = 3,
    };
};

// llc/snap header for 802.11 data payloads (rfc 1042)
constexpr auto llc_snap     = noxx::to_array<u8>({0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00});
constexpr auto llc_snap_len = usize(8); // 6 byte header + 2 byte ethertype
} // namespace halow::dot11
