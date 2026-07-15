#pragma once
#include <noxx/int.hpp>

namespace crypto::dot1x {
struct Header {
    struct Type {
        enum : u8 {
            Packet = 0,
            Start  = 1,
            Logoff = 2,
            Key    = 3,
        };
    };

    u8  version;
    u8  type;
    u16 length;
} __attribute__((packed));

// ref struct wpa_eapol_key
struct KeyPacket {
    // ref EAPOL_KEY_TYPE_*
    struct Type {
        enum : u8 {
            RC4 = 1,
            RSN = 2,
            WPA = 254,
        };
    };

    // ref WPA_KEY_INFO_*
    struct InfoType {
        enum : u8 {
            AKMDefined  = 0,
            HMACMD5RC4  = 1,
            HMACSHA1AES = 2,
            AES128CMAC  = 3,
        };
    };
    struct KeyType {
        enum : u8 {
            GroupKey = 0,
            Pairwise = 1,
        };
    };
    struct Info {
        enum : u16 {
            InfoType    = 0b0000'0000'0000'0111, // InfoType
            KeyType     = 0b0000'0000'0000'1000, // KeyType
            KeyIndex    = 0b0000'0000'0011'0000,
            Install     = 0b0000'0000'0100'0000, // when KeyType == Pairwise
            TxRx        = 0b0000'0000'0100'0000, // when KeyType == GroupKey
            Ack         = 0b0000'0000'1000'0000,
            Mic         = 0b0000'0001'0000'0000,
            Secure      = 0b0000'0010'0000'0000,
            Error       = 0b0000'0100'0000'0000,
            Request     = 0b0000'1000'0000'0000,
            EncrKeyData = 0b0001'0000'0000'0000,
            SMKMessage  = 0b0010'0000'0000'0000,
        };
    };

    u8  type; // Type
    u16 info; // Info
    u16 keysize;
    u64 replay;
    u8  nonce[32];
    u8  iv[16];
    u8  rsc[8];
    u8  _reserved[8];
    u8  mic[16];
    u16 datalen;
} __attribute__((packed));
} // namespace crypto::dot1x
