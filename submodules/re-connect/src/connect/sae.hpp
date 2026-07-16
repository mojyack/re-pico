#pragma once
#include <crypto/p256.hpp>
#include <crypto/rng.hpp>
#include <noxx/span.hpp>

#include "mac-addr.hpp"

// SAE (Simultaneous Authentication of Equals) for WPA3, group 19 (P-256),
// hash-to-element PWE only (sae_pwe=1). Ref IEEE 802.11-2020 12.4 and
// hostap/src/common/sae.c. The caller (halow) wraps these payloads in 802.11
// authentication frames and drives the commit/confirm exchange.
namespace connect::sae {
constexpr auto group_id    = u16(19);
constexpr auto scalar_len  = usize(32);
constexpr auto element_len = usize(64); // x || y

bytes_alias(KCK, 32); // Key Confirmation Key
bytes_alias(PMK, 32); // Pairwise Master Key
bytes_alias(PMKID, 16);

// 802.11 status codes carried in the SAE authentication frame
constexpr auto status_success         = u16(0);
constexpr auto status_hash_to_element = u16(126);
constexpr auto status_anti_clogging   = u16(76);

// Password token: PT = SSWU(u1) + SSWU(u2), fixed per (ssid, password) and
// reusable across connections. Derived once, off the connect hot path.
struct Pt {
    crypto::p256::Point point;
};

auto derive_pt(noxx::Span<const u8> ssid, noxx::Span<const u8> password) -> Pt;

// One SAE handshake against one peer. Not copyable once started (holds the
// secret scalar). Reset by calling start() again.
struct Session {
    crypto::p256::Point pwe;  // val * PT
    crypto::Bn256       rand; // secret
    crypto::Bn256       own_scalar;
    crypto::p256::Point own_element; // -(mask * PWE)
    crypto::Bn256       peer_scalar;
    crypto::p256::Point peer_element;
    KCK                 kck;
    PMK                 pmk;
    PMKID               pmkid;
    u16                 send_confirm = 0;
    bool                committed    = false;
    bool                have_keys    = false;

    // derive PWE from PT and the two MACs, then pick a random commit scalar/element.
    auto start(const Pt& pt, MacAddrRef own_mac, MacAddrRef peer_mac, crypto::Rng& rng) -> bool;

    // append the SAE commit payload: group(le16) | scalar(32) | element(64),
    // followed by an optional anti-clogging token container (H2E). returns the
    // number of bytes written, or 0 on error / insufficient space.
    auto write_commit(noxx::Span<u8> out, noxx::Span<const u8> token = {nullptr, 0}) const -> usize;

    // parse the peer's commit payload (group | scalar | element | ...),
    // validate it, and derive KCK/PMK/PMKID.
    auto read_commit(noxx::Span<const u8> payload) -> bool;

    // append the SAE confirm payload: send-confirm(le16) | confirm(32).
    // must be called after read_commit. returns bytes written or 0.
    auto write_confirm(noxx::Span<u8> out) -> usize;

    // verify the peer's confirm payload (send-confirm | confirm)
    auto verify_confirm(noxx::Span<const u8> payload) const -> bool;
};
} // namespace connect::sae
