#include <noxx/array.hpp>
#include <noxx/bits.hpp>
#include <noxx/buf-reader.hpp>
#include <noxx/buf-writer.hpp>
#include <noxx/comptime-string.hpp>
#include <noxx/endian.hpp>
#include <noxx/platform.hpp>

#include "aes-cmac.hpp"
#include "aes-keywrap.hpp"
#include "dot1x.hpp"
#include "ie.hpp"
#include "eapol.hpp"
#include "kdf.hpp"

#include <noxx/assert.hpp>

namespace crypto::eapol {
namespace {
constexpr auto error_value = usize(0);

// parse the GTK / IGTK KDEs out of the decrypted key data
auto parse_key_data(const noxx::Span<const u8> kd, GroupKeys& out) -> bool {
    auto r = noxx::BufReader::from_span(kd);
    while(r.size >= 2) {
        unwrap(id, r.read<u8>());
        unwrap(len, r.read<u8>());
        unwrap(body, r.read_span(len));
        if(id == ie::Id::VendorSpecific) {
            auto r = noxx::BufReader::from_span(body);
            unwrap(oui, r.read_span(kde::rsn_oui.size()));
            if(oui != kde::rsn_oui) {
                continue;
            }
            unwrap(type, r.read<u8>());
            switch(type) {
            case kde::Type::Gtk: {
                unwrap(id, r.read<u8>());
                ensure(r.read(1)); // reserved
                out.gtk.key_id = id & 0x03;
                ensure(r.size <= gtk_max);
                out.gtk.len = r.size;
                noxx::memcpy(out.gtk.key, r.data, r.size);
            } break;
            case kde::Type::Igtk: {
                unwrap(id, r.read<u16>());
                unwrap(ipn, r.read<Igtk::IPN>());
                unwrap(key, r.read<Igtk::Key>());
                out.igtk.emplace(Igtk{
                    .key    = key,
                    .ipn    = ipn,
                    .key_id = id,
                });
            } break;
            }
        }
    }
    return out.gtk.len != 0;
}

auto build_reply(const noxx::Span<u8>       out,
                 const u8                   version,
                 const u16                  key_info,
                 const u64                  replay,
                 const NonceRef             nonce,
                 const noxx::Span<const u8> key_data,
                 const Aes128::KeyRef       kck) -> usize {
    auto w = noxx::BufWriter::from_span(out);
    unwrap(header, w.alloc_obj<dot1x::Header>());
    unwrap(kp, w.alloc_obj<dot1x::KeyPacket>());
    unwrap(kd, w.alloc(key_data.size()));

    header = dot1x::Header{
        .version = version,
        .type    = dot1x::Header::Type::Key,
        .length  = noxx::byteswap(u16(sizeof(dot1x::KeyPacket) + key_data.size())),
    };

    kp = dot1x::KeyPacket{
        .type    = dot1x::KeyPacket::Type::RSN,
        .info    = noxx::byteswap(key_info),
        .keysize = 0,
        .replay  = noxx::byteswap(replay),
        .datalen = noxx::byteswap(u16(key_data.size())),
    };
    if(nonce.valid()) {
        noxx::memcpy(kp.nonce, nonce.data, nonce.size());
    }

    if(key_data.valid()) {
        noxx::memcpy(&kd, key_data.data, key_data.size());
    }

    aes_cmac(Aes128(kck), {out.data, w.data - out.data}, Aes128::BlockMutRef(kp.mic));

    return w.data - out.data;
}
} // namespace

auto derive_ptk(const noxx::Span<const u8> pmk, const MacAddrRef aa, const MacAddrRef spa, const NonceRef anonce, const NonceRef snonce) -> Ptk {
    // min|max ordering of the two MACs and the two nonces
    const auto mac_swap = noxx::memcmp(aa.data, spa.data, aa.size()) < 0;
    const auto mac_lo   = mac_swap ? aa : spa;
    const auto mac_hi   = mac_swap ? spa : aa;
    const auto n_swap   = noxx::memcmp(anonce.data, snonce.data, anonce.size()) < 0;
    const auto n_lo     = n_swap ? anonce : snonce;
    const auto n_hi     = n_swap ? snonce : anonce;

    auto data = noxx::Array<u8, aa.size() + spa.size() + anonce.size() + snonce.size()>();
    auto w    = noxx::BufWriter::from_span(data);
    w.append_span(mac_lo);
    w.append_span(mac_hi);
    w.append_span(n_lo);
    w.append_span(n_hi);

    auto           ret   = Ptk();
    constexpr auto label = noxx::comptime::String("Pairwise key expansion");
    sha256_prf(pmk, {(const u8*)label.data, label.size()}, data, noxx::Span<u8>{(u8*)&ret, sizeof(ret)});
    return ret;
}

auto Supplicant::on_frame(const noxx::Span<const u8> in, const noxx::Span<u8> out) -> usize {
    auto r = noxx::BufReader::from_span(in);
    unwrap(header, r.read<dot1x::Header>());
    unwrap(kp, r.read<dot1x::KeyPacket>());
    ensure(header.type == dot1x::Header::Type::Key, "not an rsn eapol-key");
    ensure(kp.type == dot1x::KeyPacket::Type::RSN, "not an rsn eapol-key");
    unwrap(kd, r.read_span(noxx::byteswap(kp.datalen)), "truncated key data");

    const auto key_info = noxx::byteswap(kp.info);
    ensure(FB(dot1x::KeyPacket::Info::KeyType, key_info) == dot1x::KeyPacket::KeyType::Pairwise, "not a pairwise handshake frame");

    if((key_info & dot1x::KeyPacket::Info::Mic) == 0) {
        // ---- message 1: ANonce, no MIC. reply with M2 ----
        ensure(key_info & dot1x::KeyPacket::Info::Ack, "m1 without ack");
        noxx::memcpy(anonce.data, kp.nonce, anonce.size());
        replay = noxx::byteswap(kp.replay);
        ensure((*rng)(snonce), "rng failed");
        ptk      = derive_ptk(pmk, aa, spa, anonce, snonce);
        ptk_done = true;

        const auto reply_info = BF(dot1x::KeyPacket::Info::InfoType, dot1x::KeyPacket::InfoType::AKMDefined) |
                                BF(dot1x::KeyPacket::Info::KeyType, dot1x::KeyPacket::KeyType::Pairwise) |
                                dot1x::KeyPacket::Info::Mic;
        return build_reply(out, header.version, reply_info, replay, snonce, rsn_ie, ptk.kck);
    }

    // ---- message 3: MIC + encrypted key data. reply with M4 ----
    ensure(ptk_done, "m3 before m1");
    ensure(key_info & dot1x::KeyPacket::Info::EncrKeyData, "m3 key data not encrypted");
    ensure(noxx::memcmp(kp.nonce, anonce.data, anonce.size()) == 0, "m3 anonce mismatch");
    ensure(noxx::byteswap(kp.replay) > replay, "m3 replay not fresh");

    // verify mic
    // - save original mic
    auto orig_mic = noxx::Array<u8, sizeof(dot1x::KeyPacket::mic)>();
    noxx::memcpy(orig_mic.data, kp.mic, sizeof(kp.mic));
    // - clear mic for re-compute
    noxx::memset((u8*)kp.mic, 0, sizeof(kp.mic)); //  FIXME: this overwriting const u8
    // - compute mac
    auto got_mic = noxx::Array<u8, sizeof(dot1x::KeyPacket::mic)>();
    aes_cmac(Aes128(ptk.kck), {in.data, r.data - in.data}, got_mic);
    // - compare them
    ensure(orig_mic == got_mic, "m3 mic mismatch");

    // decrypt (AES key unwrap) the key data with the KEK
    ensure(kd.size() >= 24 && kd.size() % 8 == 0, "bad m3 key data length");
    auto plain = noxx::Array<u8, 512>();
    ensure(aes_key_unwrap(Aes128(ptk.kek), kd, plain.data), "m3 key unwrap failed");
    ensure(parse_key_data({plain.data, kd.size() - 8}, group), "no gtk in m3");

    replay                = noxx::byteswap(kp.replay);
    const auto reply_info = BF(dot1x::KeyPacket::Info::InfoType, dot1x::KeyPacket::InfoType::AKMDefined) |
                            BF(dot1x::KeyPacket::Info::KeyType, dot1x::KeyPacket::KeyType::Pairwise) |
                            dot1x::KeyPacket::Info::Mic |
                            dot1x::KeyPacket::Info::Secure;

    const auto n = build_reply(out, header.version, reply_info, replay, {}, {}, ptk.kck);
    complete     = n != 0;
    return n;
}
} // namespace crypto::eapol
