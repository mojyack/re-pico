#pragma once
#include <noxx/array.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>

#include "dot1x.hpp"
#include "mac-addr.hpp"
#include "rng.hpp"
#include "util.hpp"

// RSN 4-way handshake supplicant for AKM SAE (00-0F-AC:8): PTK derivation,
// EAPOL-Key MIC (AES-CMAC) and key-data unwrap (AES key wrap), and GTK/IGTK
// extraction. Ref IEEE 802.11-2020 12.7 and hostap/src/rsn_supp/wpa.c.
namespace crypto::eapol {
bytes_alias(Nonce, sizeof(dot1x::KeyPacket::nonce));

constexpr auto gtk_max = usize(32);

// ref RSN_KEY_DATA_*
// ref IEEE 802.11-2020 12.7.2 table 12-9
namespace kde {
constexpr auto rsn_oui = noxx::Array<u8, 3>{0x00, 0x0f, 0xac};

struct Type {
    enum : u8 {
        Gtk            = 1,
        StaKey         = 2,
        MacAddr        = 3,
        Pmkid          = 4,
        Igtk           = 9,
        KeyId          = 10,
        MultibandGtk   = 11,
        MultibandKeyId = 12,
        Oci            = 13,
        Bigtk          = 14,
        MloGtk         = 16,
        MloIgtk        = 17,
        MloBigtk       = 18,
        MloLink        = 19,
    };
};
} // namespace kde

struct Ptk {
    bytes_alias(KCK, 16);
    bytes_alias(KEK, 16);
    bytes_alias(TK, 16); // CCMP-128

    KCK kck;
    KEK kek;
    TK  tk;
};
static_assert(sizeof(Ptk) == 48);

// PTK = KDF(pmk, "Pairwise key expansion",
//           min(aa,spa)|max(aa,spa) | min(anonce,snonce)|max(anonce,snonce))
auto derive_ptk(noxx::Span<const u8> pmk, MacAddrRef aa, MacAddrRef spa, NonceRef anonce, NonceRef snonce) -> Ptk;

struct Gtk {
    u8 key[gtk_max];
    u8 len;
    u8 key_id;
};

struct Igtk {
    bytes_alias(Key, 16);
    bytes_alias(IPN, 6);

    Key key;
    IPN ipn;
    u16 key_id;
};

// group keys unwrapped from M3; igtk present only when the AP uses management
// frame protection (ieee80211w)
struct GroupKeys {
    Gtk                  gtk;
    noxx::Optional<Igtk> igtk;
};

// Drives the supplicant side of the 4-way handshake, one message at a time.
struct Supplicant {
    // configuration — set before feeding the first frame
    noxx::Span<const u8> pmk;
    noxx::Span<const u8> rsn_ie; // our RSN IE, sent in M2
    MacAddrRef           aa;     // authenticator (AP) MAC
    MacAddrRef           spa;    // supplicant (our) MAC
    Rng*                 rng;

    // outputs
    Ptk       ptk;
    GroupKeys group;
    bool      ptk_done = false;
    bool      complete = false; // M4 sent, keys ready to install

    // internal
    Nonce anonce;
    Nonce snonce;
    u64   replay;

    // feed a received EAPOL-Key frame (from the 802.1X version byte onward).
    // writes the reply (M2 for M1, M4 for M3) into out and returns its length.
    // returns 0 on a frame that needs no reply, is malformed, or fails
    // validation (bad MIC, replay, key unwrap).
    auto on_frame(noxx::Span<const u8> in, noxx::Span<u8> out) -> usize;
};
} // namespace crypto::eapol
