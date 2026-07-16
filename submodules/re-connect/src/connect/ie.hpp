#pragma once
#include <noxx/int.hpp>

namespace connect::ie {
struct Id {
    enum : u8 {
        Ssid            = 0,
        AidRequest      = 210,
        AidResponse     = 211,
        ShortBcnInt     = 214,
        S1gCapabilities = 217, // S1gCaps
        VendorSpecific  = 221,
        S1gOperation    = 232, // S1gOp
    };
};

struct Header {
    u8 id;
    u8 length;
    // u8 data[];
} __attribute__((packed));

// s1g mcs map: 2-bit max-mcs per nss (DOT11_S1G_NSS_MAX_*)
struct S1gNssMaxMcs {
    enum : u8 {
        Mcs2 = 0,
        Mcs7 = 1,
        Mcs9 = 2,
        None = 3,
    };
};

// s1g capabilities ie body (struct dot11_ie_s1g_capabilities)
struct S1gCaps {
    // s1g capabilities information bits (DOT11_S1G_CAP*, one enum per info byte)
    struct Cap0 {
        enum : u8 {
            SuppWidth12Mhz     = 0 << 6,
            SuppWidth124Mhz    = 1 << 6,
            SuppWidth1248Mhz   = 2 << 6,
            SuppWidth124816Mhz = 3 << 6,
        };
    };

    struct Cap4 {
        enum : u8 {
            StaTypeSensor    = 1 << 6,
            StaTypeNonSensor = 2 << 6,
        };
    };

    struct Cap7 {
        enum : u8 {
            Dup1MhzSupport = 1 << 1,
        };
    };

    Header header;
    u8     s1g_capabilities_info[10]; // Cap<n> bits per byte
    u8     supported_s1g_mcs_and_nss_set[5];
} __attribute__((packed));
static_assert(sizeof(S1gCaps) == 17);

// s1g operation ie body (struct dot11_ie_s1g_operation)
struct S1gOp {
    // s1g operation channel width byte bits (DOT11_MASK_S1G_OP_CHAN_WIDTH_*)
    struct ChanWidth {
        enum : u8 {
            PrimIs1Mhz = 0b0000'0001, // set: 1MHz primary, clear: 2MHz primary
            OpWidth    = 0b0001'1110, // operating width minus one
            PrimLoc    = 0b0010'0000, // upper/lower 1MHz of the primary channel
            NoMcs10    = 0b1000'0000,
        };
    };

    Header header;
    u8     channel_width; // ChanWidth
    u8     operating_class;
    u8     primary_channel_number;
    u8     channel_center_freq;
    u8     basic_s1g_mcs_nss_set[2];
} __attribute__((packed));
static_assert(sizeof(S1gOp) == 8);
} // namespace connect::ie
