#include <noxx/array.hpp>
#include <noxx/bits.hpp>
#include <noxx/buf-reader.hpp>
#include <noxx/buf-writer.hpp>
#include <noxx/endian.hpp>

#include "connect/dot1x.hpp"
#include "connect/eapol.hpp"
#include "connect/ie.hpp"
#include "crypto/aes-cmac.hpp"
#include "crypto/aes-keywrap.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace {
constexpr auto error_value = false;

namespace eapol = connect::eapol;

// a deterministic RNG that always returns the scripted SNonce
struct FixedRng : crypto::Rng {
    const u8* value;

    auto operator()(const noxx::Span<u8> out) -> bool override {
        for(auto i = usize(0); i < out.size(); i += 1) {
            out[i] = value[i];
        }
        return true;
    }
};

const auto aa  = connect::MacAddr{0x00, 0x60, 0xad, 0x80, 0x1f, 0x51};
const auto spa = connect::MacAddr{0x0c, 0xbf, 0x74, 0x00, 0x00, 0x0a};

// build a raw EAPOL-Key frame; key_data is already plaintext-or-wrapped bytes
auto build_frame(const noxx::Span<u8>           out,
                 const u16                      key_info,
                 const u64                      replay,
                 const connect::eapol::NonceRef nonce,
                 const noxx::Span<const u8>     key_data,
                 const crypto::Aes128::Key*     kck_for_mic) -> usize {
    auto w = noxx::SpanWriter(out);
    unwrap(header, w.alloc_obj<connect::dot1x::Header>());
    unwrap(kp, w.alloc_obj<connect::dot1x::KeyPacket>());
    unwrap(kd, w.alloc(key_data.size()));

    header = {
        .version = 2,
        .type    = connect::dot1x::Header::Type::Key,
        .length  = noxx::byteswap(u16(key_data.size())),
    };
    kp = {
        .type    = connect::dot1x::KeyPacket::Type::RSN,
        .info    = noxx::byteswap(key_info),
        .keysize = noxx::byteswap(u16(16)), // CCMP
        .replay  = noxx::byteswap(replay),
        .datalen = noxx::byteswap(u16(key_data.size())),
    };
    noxx::memcpy(kp.nonce, nonce.data, nonce.size());
    noxx::memcpy(&kd, key_data.data, key_data.size());
    if(kck_for_mic != nullptr) {
        crypto::aes_cmac(crypto::Aes128(*kck_for_mic), {out.data, w.buf.data - out.data}, crypto::Aes128::BlockMutRef{kp.mic});
    }
    return w.buf.data - out.data;
}

// append a GTK or IGTK 00-0F-AC KDE holding key
auto append_gtk_kde(noxx::SpanWriter& w, const u8 type, const u16 id_byte, const noxx::Span<const u8> key) -> void {
    const auto orig = w.buf.data;

    w.append_obj(u8(connect::ie::Id::VendorSpecific));
    auto& len = *w.alloc_obj<u8>(); // later
    w.append_obj(connect::eapol::kde::rsn_oui);
    w.append_obj(type);
    switch(type) {
    case connect::eapol::kde::Type::Gtk:
        w.append_obj(u8(id_byte));
        w.append_obj(u8(0));
        w.append_span(key);
        break;
    case connect::eapol::kde::Type::Igtk:
        w.append_obj(id_byte);
        w.append_obj(connect::eapol::Igtk::IPN());
        w.append_span(key);
        break;
    }
    len = w.buf.data - orig - 2 /* exclude id and len */;
}

auto verify_mic(const noxx::Span<const u8> m, const connect::eapol::Ptk::KCK& kck) -> bool {
    auto r = noxx::SpanReader(m);
    unwrap(header, r.read<connect::dot1x::Header>());
    (void)header;
    unwrap(kp, r.read<connect::dot1x::KeyPacket>());
    auto orig_mic = noxx::Array<u8, sizeof(connect::dot1x::KeyPacket::mic)>();
    noxx::memcpy(orig_mic.data, kp.mic, sizeof(kp.mic));
    noxx::memset((u8*)kp.mic, 0, sizeof(kp.mic)); // FIXME: this overwriting const u8
    auto got_mic = noxx::Array<u8, sizeof(connect::dot1x::KeyPacket::mic)>();
    crypto::aes_cmac(crypto::Aes128(kck), m, got_mic);
    ensure(orig_mic == got_mic, "MIC invalid");
    return true;
}

// full 4-way exchange against a mock authenticator that mirrors the supplicant's
// crypto; also checks the PTK against the independent Python oracle
auto full_handshake() -> bool {
    auto pmk_arr = noxx::Array<u8, 32>();
    ensure(test::from_hex("a45db8a0b3725a6ac7e9fedec3721d1d594998a1167443c32d8ca3edb8f45753", pmk_arr) == 32);
    auto anonce = connect::eapol::Nonce();
    auto snonce = connect::eapol::Nonce();
    for(auto i = 0uz; i < connect::eapol::Nonce::size(); i += 1) {
        anonce[i] = 0x1a;
        snonce[i] = 0x2b;
    }

    // RSN IE the supplicant sends in M2 (opaque to the handshake)
    const auto rsn_ie = noxx::to_array<u8>({0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x08, 0xc0, 0x00});

    auto rng   = FixedRng();
    rng.value  = snonce.data;
    auto sta   = eapol::Supplicant();
    sta.pmk    = pmk_arr;
    sta.rsn_ie = rsn_ie;
    sta.aa     = aa;
    sta.spa    = spa;
    sta.rng    = &rng;

    // the authenticator derives the same PTK to build/verify MICs
    auto auth_ptk = eapol::derive_ptk({pmk_arr.data, 32}, aa, spa, anonce, snonce);

    // --- M1: ANonce, no MIC ---
    auto       m1     = noxx::Array<u8, 256>();
    const auto ki_m1  = BF(connect::dot1x::KeyPacket::Info::KeyType, connect::dot1x::KeyPacket::KeyType::Pairwise) |
                        connect::dot1x::KeyPacket::Info::Ack;
    const auto m1_len = build_frame(m1, ki_m1, 0x01, anonce, {}, nullptr);

    auto       m2     = noxx::Array<u8, 256>();
    const auto m2_len = sta.on_frame({m1.data, m1_len}, m2);
    ensure(m2_len != 0, "no M2 produced");
    ensure(sta.ptk_done);

    // PTK known-answer (independent Python oracle)
    ensure(test::matches(sta.ptk.kck, "76821d2620035eef71fa124932390a08"));
    ensure(test::matches(sta.ptk.kek, "1d1747890368f873f5028987aae0e3ea"));
    ensure(test::matches(sta.ptk.tk, "a53e4a5c416d34e5c85df421a34e432d"));

    // authenticator verifies the M2 MIC
    ensure(verify_mic({m2.data, m2_len}, auth_ptk.kck));

    // --- M3: encrypted GTK + IGTK ---
    auto gtk  = noxx::Array<u8, 16>();
    auto igtk = noxx::Array<u8, 16>();
    for(auto i = 0; i < 16; i += 1) {
        gtk[i]  = u8(0xa0 + i);
        igtk[i] = u8(0xb0 + i);
    }
    auto kd = noxx::Array<u8, 128>();
    auto w  = noxx::SpanWriter(kd);
    append_gtk_kde(w, eapol::kde::Type::Gtk, 0x01, gtk);
    append_gtk_kde(w, eapol::kde::Type::Igtk, 0x04, igtk);
    while((w.buf.data - kd.data) % 8 != 0) { // pad to an 8-byte multiple
        w.append_obj(u8(0));
    }
    const auto kd_len  = w.buf.data - kd.data;
    auto       wrapped = noxx::Array<u8, 160>();
    ensure(crypto::aes_key_wrap(crypto::Aes128(auth_ptk.kek), {kd.data, kd_len}, wrapped.data));

    auto       m3     = noxx::Array<u8, 256>();
    const auto ki_m3  = BF(connect::dot1x::KeyPacket::Info::KeyType, connect::dot1x::KeyPacket::KeyType::Pairwise) |
                        connect::dot1x::KeyPacket::Info::Install |
                        connect::dot1x::KeyPacket::Info::Ack |
                        connect::dot1x::KeyPacket::Info::Mic |
                        connect::dot1x::KeyPacket::Info::Secure |
                        connect::dot1x::KeyPacket::Info::EncrKeyData;
    const auto m3_len = build_frame(m3, ki_m3, 0x02, anonce, {wrapped.data, kd_len + 8}, &auth_ptk.kck);

    auto       m4     = noxx::Array<u8, 256>();
    const auto m4_len = sta.on_frame({m3.data, m3_len}, m4);
    ensure(m4_len != 0, "no M4 produced");
    ensure(sta.complete);

    // GTK / IGTK extracted correctly
    ensure(sta.group.gtk.len == 16 && sta.group.gtk.key_id == 1);
    ensure(test::matches({sta.group.gtk.key, 16}, "a0a1a2a3a4a5a6a7a8a9aaabacadaeaf"));
    ensure(sta.group.igtk, "IGTK missing");
    ensure((*sta.group.igtk).key_id == 4);
    ensure(test::matches((*sta.group.igtk).key, "b0b1b2b3b4b5b6b7b8b9babbbcbdbebf"));

    // authenticator verifies the M4 MIC
    ensure(verify_mic({m4.data, m4_len}, auth_ptk.kck));

    return true;
}

// a tampered M3 MIC must be rejected (no M4)
auto bad_mic_rejected() -> bool {
    auto pmk_arr = noxx::Array<u8, 32>();
    ensure(test::from_hex("a45db8a0b3725a6ac7e9fedec3721d1d594998a1167443c32d8ca3edb8f45753", pmk_arr) == 32);
    auto anonce = connect::eapol::Nonce();
    auto snonce = connect::eapol::Nonce();
    for(auto i = 0uz; i < connect::eapol::Nonce::size(); i += 1) {
        anonce[i] = 0x1a;
        snonce[i] = 0x2b;
    }
    const auto rsn_ie = noxx::Array<u8, 4>{0x30, 0x02, 0x01, 0x00};

    auto rng   = FixedRng();
    rng.value  = snonce.data;
    auto sta   = eapol::Supplicant();
    sta.pmk    = pmk_arr;
    sta.rsn_ie = rsn_ie;
    sta.aa     = aa;
    sta.spa    = spa;
    sta.rng    = &rng;

    auto auth_ptk = eapol::derive_ptk({pmk_arr.data, 32}, aa, spa, anonce, snonce);

    auto       m1     = noxx::Array<u8, 256>();
    auto       m2     = noxx::Array<u8, 256>();
    const auto ki     = BF(connect::dot1x::KeyPacket::Info::KeyType, connect::dot1x::KeyPacket::KeyType::Pairwise) |
                        connect::dot1x::KeyPacket::Info::Ack;
    const auto m1_len = build_frame(m1, ki, 0x01, anonce, {}, nullptr);
    ensure(sta.on_frame({m1.data, m1_len}, m2) != 0);

    auto kd  = noxx::Array<u8, 128>();
    auto gtk = noxx::Array<u8, 16>();
    for(auto i = 0uz; i < gtk.size(); i += 1) {
        gtk[i] = u8(i);
    }
    auto w = noxx::SpanWriter(kd);
    append_gtk_kde(w, eapol::kde::Type::Gtk, 0x01, gtk);
    while((w.buf.data - kd.data) % 8 != 0) { // pad to an 8-byte multiple
        w.append_obj(u8(0));
    }
    const auto kd_len  = w.buf.data - kd.data;
    auto       wrapped = noxx::Array<u8, 160>();
    ensure(crypto::aes_key_wrap(crypto::Aes128(auth_ptk.kek), {kd.data, kd_len}, wrapped.data));
    auto       m3     = noxx::Array<u8, 256>();
    const auto ki_m3  = BF(connect::dot1x::KeyPacket::Info::KeyType, connect::dot1x::KeyPacket::KeyType::Pairwise) |
                        connect::dot1x::KeyPacket::Info::Install |
                        connect::dot1x::KeyPacket::Info::Ack |
                        connect::dot1x::KeyPacket::Info::Mic |
                        connect::dot1x::KeyPacket::Info::Secure |
                        connect::dot1x::KeyPacket::Info::EncrKeyData;
    const auto m3_len = build_frame(m3, ki_m3, 0x02, anonce, {wrapped.data, kd_len + 8}, &auth_ptk.kck);

    ((connect::dot1x::KeyPacket*)&m3[sizeof(connect::dot1x::Header)])->mic[0] ^= 0x01; // corrupt the MIC

    auto m4 = noxx::Array<u8, 256>();
    ensure(sta.on_frame({m3.data, m3_len}, m4) == 0, "tampered M3 must be rejected");
    ensure(!sta.complete);
    return true;
}
} // namespace

auto main() -> int {
    constexpr auto error_value = 1;
    ensure(full_handshake());
    ensure(bad_mic_rejected());
    printf("pass\n");
    return 0;
}
