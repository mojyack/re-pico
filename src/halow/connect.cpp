#include <connect/eapol.hpp>
#include <connect/sae.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <crypto/rng.hpp>
#include <hal/rng.hpp>
#include <hal/time.hpp>
#include <net/ethernet.hpp>
#include <net/packet-buf.hpp>
#include <noxx/array.hpp>
#include <noxx/bits.hpp>
#include <noxx/buf-reader.hpp>
#include <noxx/buf-writer.hpp>
#include <noxx/endian.hpp>

#include "command.hpp"
#include "connect.hpp"
#include "netif.hpp"
#include "rate-code.hpp"
#include "scan.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
constexpr auto qdbm_per_dbm     = i32(4);
constexpr auto mgmt_tid         = u8(7); // MMWLAN_MAX_QOS_TID
constexpr auto mgmt_attempts    = u8(5);
constexpr auto auth_tries       = u32(3);
constexpr auto response_wait_us = u64(1'000'000);
constexpr auto assoc_capability = u16(dot11::CapInfo::Privacy | dot11::CapInfo::ShortPreamble |
                                      dot11::CapInfo::Qos | dot11::CapInfo::ShortSlotTime);
constexpr auto eapol_timeout_us = u64(5'000'000); // covers a couple of ap m1 retransmits

// RSN information element advertised in the (secure) association request and
// echoed in EAPOL message 2: WPA3-SAE with CCMP pairwise/group and required
// management frame protection (BIP-CMAC-128 group mgmt cipher). must match the
// ap's `wpa_key_mgmt=SAE ieee80211w=2` config.
constexpr auto rsn_ie = noxx::to_array<u8>({
    // clang-format off
    48, 26,                  // element id, length
    0x01, 0x00,              // version 1
    0x00, 0x0f, 0xac, 0x04,  // group data cipher: CCMP-128
    0x01, 0x00,              // pairwise cipher count
    0x00, 0x0f, 0xac, 0x04,  // pairwise cipher: CCMP-128
    0x01, 0x00,              // akm suite count
    0x00, 0x0f, 0xac, 0x08,  // akm: SAE
    0xc0, 0x00,              // rsn capabilities: MFPR | MFPC
    0x00, 0x00,              // pmkid count
    0x00, 0x0f, 0xac, 0x06,  // group management cipher: BIP-CMAC-128
    // clang-format on
});
// RSN extension element: SAE hash-to-element used (bit 5)
constexpr auto rsnxe = noxx::to_array<u8>({244, 1, 0x20});

auto link   = LinkStatus();
auto seq    = u16(0); // 802.11 sequence number, shared space
auto pkt_id = u32(0); // tx status correlation

// hardware TRNG bridged to the crypto stack's Rng interface, used by SAE and
// the EAPOL supplicant for the secret scalar / SNonce
struct HwRng : crypto::Rng {
    auto operator()(noxx::Span<u8> out) -> bool override {
        return rng::fill(out);
    }
};
auto hw_rng = HwRng();

// PMK handed from the SAE exchange to the 4-way handshake within one connect()
auto sae_pmk = connect::sae::PMK();

// primary channel width and rate for the current link, from the s1g operation ie
auto prim_bw_mhz = u8(1);

// link maintenance task state (see link_task)
constexpr auto keepalive_interval_us = u64(30'000'000); // idle time before a keepalive, well under the ap inactivity timeout
constexpr auto link_wake_us          = u64(500'000);    // rx wait slice, bounds event/keepalive/desync check latency

auto last_activity = u64(0);             // last rx/tx time, drives the keepalive timer
auto link_handle   = coop::TaskHandle(); // link_task's handle, cancelled by disconnect

auto mgmt_rate() -> u32 {
    return make_s1g_rate_code(prim_bw_mhz == 1 ? RateBw::Bw1Mhz : RateBw::Bw2Mhz, 0, 0);
}

auto make_mgmt_header(const u16 frame_control, const dot11::MacAddr& addr1, const dot11::MacAddr& addr2, const dot11::MacAddr& addr3) -> dot11::Header {
    seq = (seq + 1) & 0xfff;
    return {
        .frame_control    = frame_control,
        .duration         = 0,
        .addr1            = addr1,
        .addr2            = addr2,
        .addr3            = addr3,
        .sequence_control = u16(seq << 4),
    };
}

// send a management frame with the fixed low-rate tx parameters
auto tx_mgmt(const noxx::Span<const u8> frame) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);
    co_ensure(net::PacketWriter(*packet).append_span(frame), "mgmt frame too long");

    pkt_id += 1;
    const auto info = SkbHeader::TxInfo{
        .flags  = TxFlag::ImmediateReport | BF(TxFlag::VifId, u32(link.vif)),
        .pkt_id = pkt_id,
        .tid    = mgmt_tid,
        .rates  = {{.rate_code = mgmt_rate(), .count = mgmt_attempts}},
    };
    co_return co_await yaps_tx(SkbChan::Mgmt, *packet, &info);
}

// await a mgmt frame of the given type from our bss, discarding others
auto wait_mgmt(const u16 frame_control, const u64 timeout_us) -> coop::Async<noxx::Optional<net::AutoPacket>> {
    constexpr auto error_value = noxx::nullopt;

    const auto deadline = time::now() + timeout_us;
    while(true) {
        const auto now = time::now();
        if(now >= deadline) {
            break;
        }
        auto rx = co_await wait_rx(deadline - now);
        if(!rx) {
            continue; // timed out, the loop condition ends the wait
        }
        co_unwrap(skbh, parse_skb_header(*rx));
        auto r = noxx::SpanReader(noxx::Span<const u8>{packet_frame(*rx, skbh), skbh.len});
        co_unwrap(header, r.read<dot11::Header>());
        if((header.frame_control & dot11::Fc::VerTypeSubMask) != frame_control) {
            log<"halow: connect skipping frame fc 0x{04x}\n">(header.frame_control);
            continue;
        }
        if(!mac_equal(header.addr2.data, link.bssid.data)) {
            continue;
        }
        co_return noxx::move(rx);
    }
    co_return noxx::nullopt;
}

auto set_sta_state(const u16 state, const u16 aid) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto req = SetStaStateReq{
        .sta_addr = link.bssid,
        .aid      = aid,
        .state    = state,
    };
    co_return bool(co_await send_command(CommandId::SetStaState, as_span(req), {}, link.vif));
}

// SET_CHANNEL + SET_TXPOWER from the ap's s1g operation ie
auto configure_channel(const connect::ie::S1gOp& s1g_op) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto regdom = find_regdom();
    co_ensure(regdom != nullptr);
    auto op_chan  = (const S1gChannel*)nullptr;
    auto pri_chan = (const S1gChannel*)nullptr;
    for(auto i = u32(0); i < regdom->num_channels; i += 1) {
        const auto& channel = regdom->channels[i];
        if(channel.chan_num == s1g_op.channel_center_freq) {
            op_chan = &channel;
        }
        if(channel.chan_num == s1g_op.primary_channel_number) {
            pri_chan = &channel;
        }
    }
    co_ensure(op_chan != nullptr && pri_chan != nullptr, "ap channel not in regdom");

    const auto width  = s1g_op.channel_width;
    const auto op_bw  = FB(connect::ie::S1gOp::ChanWidth::OpWidth, width) + 1;
    const auto pri_bw = FB(connect::ie::S1gOp::ChanWidth::PrimIs1Mhz, width) ? 1 : 2;
    co_ensure(op_bw == op_chan->bw_mhz, "op width mismatch with regdom");

    // index of the primary 1MHz subchannel within the operating channel
    // (ref umac_interface_calc_pri_1mhz_idx)
    auto pri_idx = (i32(pri_chan->freq_khz) - i32(op_chan->freq_khz) + i32(op_bw - pri_bw) * 500) / 1000;
    if(pri_bw == 2 && (width & connect::ie::S1gOp::ChanWidth::PrimLoc)) {
        pri_idx += 1;
    }
    co_ensure(pri_idx >= 0 && pri_idx < op_bw, "invalid primary channel index");

    log<"halow: channel {}khz bw {}MHz primary {}MHz idx {}\n">(op_chan->freq_khz, op_bw, pri_bw, u32(pri_idx));

    const auto req = SetChannelReq{
        .op_chan_freq_hz   = op_chan->freq_khz * 1000,
        .op_bw_mhz         = u8(op_bw),
        .pri_bw_mhz        = u8(pri_bw),
        .pri_1mhz_chan_idx = u8(pri_idx),
        .dot11_mode        = Dot11ProtoMode::Ah,
    };
    co_ensure(co_await send_command(CommandId::SetChannel, as_span(req), {}));

    const auto pwr_req = SetTxPowerReq{.power_qdbm = i32(op_chan->max_tx_eirp_dbm) * qdbm_per_dbm};
    co_ensure(co_await send_command(CommandId::SetTxPower, as_span(pwr_req), {}));

    link.freq_khz = op_chan->freq_khz;
    prim_bw_mhz   = pri_bw;
    co_return true;
}

// default edca parameters per access category; the fw data queues do not
// transmit until configured (ref populate_default_qos_queue_params)
auto configure_qos() -> coop::Async<bool> {
    constexpr auto error_value = false;

    struct QosParams {
        u8  aifs;
        u16 cw_min;
        u16 cw_max;
    };
    constexpr auto defaults = noxx::to_array<QosParams>({
        {3, 15, 1023}, // best effort
        {7, 15, 1023}, // background
        {2, 7, 15},    // video
        {2, 3, 7},     // voice
    });

    constexpr auto txop_max_us = u32(15008);

    for(auto aci = u32(0); aci < defaults.size(); aci += 1) {
        const auto req = SetQosParamsReq{
            .queue_idx             = u8(aci),
            .aifs_slot_count       = defaults[aci].aifs,
            .contention_window_min = defaults[aci].cw_min,
            .contention_window_max = defaults[aci].cw_max,
            .max_txop_usec         = txop_max_us,
        };
        co_ensure(co_await send_command(CommandId::SetQosParams, as_span(req), {}));
    }
    co_return true;
}

auto authenticate() -> coop::Async<bool> {
    constexpr auto error_value = false;

    // open system authentication, sequence 1 (ref frame_authentication_build)
    const auto req = dot11::Auth{
        .header      = make_mgmt_header(dot11::Fc::Auth, link.bssid, link.mac, link.bssid),
        .alg         = dot11::AuthAlg::Open,
        .seq         = 1,
        .status_code = dot11::status_success,
    };

    for(auto attempt = u32(0); attempt < auth_tries; attempt += 1) {
        co_ensure(co_await tx_mgmt(as_span(req)));
        auto rx_o = co_await wait_mgmt(dot11::Fc::Auth, response_wait_us);
        if(!rx_o) {
            log<"halow: auth response timeout, retrying\n">();
            continue;
        }

        co_unwrap(skbh, parse_skb_header(**rx_o));
        auto r = noxx::SpanReader(noxx::Span<const u8>{packet_frame(**rx_o, skbh), skbh.len});
        co_unwrap(auth, r.read<dot11::Auth>(), "short auth response");
        co_ensure(auth.alg == dot11::AuthAlg::Open && auth.seq == 2, "unexpected auth response");
        co_ensure(auth.status_code == dot11::status_success, "authentication refused");
        co_return true;
    }
    co_ensure(false, "authentication timed out");
}

// build and send an SAE authentication frame (alg 3) carrying a commit or
// confirm payload at the given sequence number and status code
auto tx_sae_auth(const u16 seq_num, const u16 status_code, const noxx::Span<const u8> payload) -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto frame = noxx::Array<u8, 220>();
    auto w     = noxx::SpanWriter(frame);
    co_ensure(w.append_obj(dot11::Auth{
        .header      = make_mgmt_header(dot11::Fc::Auth, link.bssid, link.mac, link.bssid),
        .alg         = dot11::AuthAlg::Sae,
        .seq         = seq_num,
        .status_code = status_code,
    }));
    co_ensure(w.append_span(payload));
    co_return co_await tx_mgmt({frame.data, w.buf.data - frame.data});
}

// receive the next SAE authentication frame from our bss at the given sequence,
// returning its payload (after the 3-word fixed body, fcs stripped)
auto wait_sae_auth(const u16 seq_num, u16& status_code) -> coop::Async<noxx::Optional<noxx::Vector<u8>>> {
    constexpr auto error_value = noxx::nullopt;

    co_unwrap(rx, co_await wait_mgmt(dot11::Fc::Auth, response_wait_us), "sae auth timeout");
    co_unwrap(skbh, parse_skb_header(*rx));
    auto r = noxx::SpanReader(noxx::Span<const u8>{packet_frame(*rx, skbh), skbh.len});
    co_unwrap(auth, r.read<dot11::Auth>(), "short sae auth frame");
    co_ensure(auth.alg == dot11::AuthAlg::Sae, "unexpected auth alg");
    co_ensure(auth.seq == seq_num, "unexpected sae auth sequence");
    if(skbh.rx_status.flags & RxFlag::FcsIncluded) {
        co_ensure(r.buf.size >= dot11::fcs_len);
        r.buf.size -= dot11::fcs_len;
    }
    status_code = auth.status_code;

    static auto payload = noxx::Array<u8, 256>();
    co_ensure(r.buf.size <= payload.size());
    auto ret = noxx::Vector<u8>();
    co_ensure(ret.resize(r.buf.size));
    noxx::memcpy(ret.data(), r.buf.data, r.buf.size);
    co_return ret;
}

// WPA3-SAE (hash-to-element) authentication: commit/confirm exchange, leaving
// the negotiated PMK in sae_pmk for the 4-way handshake (ref 802.11-2020 12.4)
auto sae_authenticate(const noxx::StringView ssid, const noxx::StringView password) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto pt      = connect::sae::derive_pt(ssid, password);
    auto       session = connect::sae::Session();
    co_ensure(session.start(pt, link.mac, link.bssid, hw_rng), "sae start failed");

    auto commit     = noxx::Array<u8, 200>();
    auto commit_len = session.write_commit(commit);
    co_ensure(commit_len != 0, "sae write_commit failed");

    // commit exchange, retrying on timeout and on an anti-clogging token request
    auto got_commit = false;
    for(auto attempt = u32(0); attempt < auth_tries && !got_commit; attempt += 1) {
        co_ensure(co_await tx_sae_auth(1, connect::sae::status_hash_to_element, {commit.data, commit_len}));

        auto status  = u16(0);
        auto payload = co_await wait_sae_auth(1, status);
        if(!payload) {
            log<"halow: sae commit timeout, retrying\n">();
            continue;
        }
        if(status == connect::sae::status_anti_clogging) {
            // body is group(le16) followed by the anti-clogging token
            co_ensure(payload->size() > 2, "empty anti-clogging token");
            commit_len = session.write_commit(commit, noxx::Span(*payload).subspan(2));
            co_ensure(commit_len != 0, "sae token rewrite_commit failed");
            log<"halow: sae anti-clogging token, resending commit\n">();
            continue;
        }
        co_ensure(status == connect::sae::status_hash_to_element || status == dot11::status_success, "sae commit refused");
        co_ensure(session.read_commit(*payload), "sae read_commit failed");
        got_commit = true;
    }
    co_ensure(got_commit, "sae commit exchange failed");

    // confirm exchange
    auto       confirm = noxx::Array<u8, 64>();
    const auto n       = session.write_confirm(confirm);
    co_ensure(n != 0, "sae write_confirm failed");
    co_ensure(co_await tx_sae_auth(2, dot11::status_success, {confirm.data, n}));

    auto status = u16(0);
    co_unwrap(payload, co_await wait_sae_auth(2, status), "no sae confirm");
    co_ensure(status == dot11::status_success, "sae confirm refused");
    co_ensure(session.verify_confirm(payload), "sae confirm verify failed");

    sae_pmk = session.pmk;
    log<"halow: sae authenticated\n">();
    co_return true;
}

auto install_key(const u8 key_idx, const u8 cipher, const u8 key_length, const u8 key_type, const noxx::Span<const u8> key, const u64 pn) -> coop::Async<bool> {
    constexpr auto error_value = false;

    auto req = InstallKeyReq{
        .pn         = pn,
        .aid        = link.aid,
        .key_idx    = key_idx,
        .cipher     = cipher,
        .key_length = key_length,
        .key_type   = key_type,
    };
    co_ensure(key.size() <= sizeof(req.key));
    noxx::memcpy(req.key, key.data, key.size());
    auto resp = InstallKeyResp{};
    co_ensure(co_await send_command(CommandId::InstallKey, as_span(req), {(u8*)&resp, sizeof(resp)}, link.vif),
              "install key command failed");
    co_return true;
}

// RSN 4-way handshake: drive the eapol supplicant over the datapath (EtherType
// 0x888E data frames), then install the pairwise/group keys (ref 802.11 12.7)
auto four_way_handshake() -> coop::Async<bool> {
    constexpr auto error_value = false;

    // key data of message 2 carries our RSN IE and RSNXE, exactly as sent in the
    // association request; the ap rejects a mismatch (downgrade protection)
    auto rsn_buf = noxx::Array<u8, rsn_ie.size() + rsnxe.size()>();
    noxx::memcpy(rsn_buf.data, rsn_ie.data, rsn_ie.size());
    noxx::memcpy(rsn_buf.data + rsn_ie.size(), rsnxe.data, rsnxe.size());

    auto supp   = connect::eapol::Supplicant();
    supp.pmk    = sae_pmk;
    supp.rsn_ie = rsn_buf;
    supp.aa     = link.bssid;
    supp.spa    = link.mac;
    supp.rng    = &hw_rng;

    const auto deadline = time::now() + eapol_timeout_us;
    while(!supp.complete) {
        const auto now = time::now();
        if(now >= deadline) {
            break;
        }
        auto rx = co_await wait_rx(deadline - now);
        if(!rx) {
            continue; // timed out, the loop condition ends the wait
        }
        // decode the data frame in place, then pick out eapol by ethertype
        if(!eth_from_rx(*rx)) {
            continue;
        }
        if(rx->len < sizeof(net::EthernetHeader)) {
            continue;
        }
        const auto eth = (const net::EthernetHeader*)rx->data();
        if(noxx::byteswap(eth->ethertype) != net::EtherType::Eapol) {
            continue;
        }

        const auto payload = noxx::Span<const u8>{rx->data() + sizeof(net::EthernetHeader), rx->len - sizeof(net::EthernetHeader)};
        auto       reply   = noxx::Array<u8, 256>();
        const auto reply_n = supp.on_frame(payload, reply);
        if(reply_n == 0) {
            continue;
        }
        co_ensure(co_await eth_tx(link.bssid, net::EtherType::Eapol, {reply.data, reply_n}), "eapol reply tx failed");
    }
    co_ensure(supp.complete, "4-way handshake timed out");

    // pairwise key first (key index 0), then the group key(s) unwrapped from m3
    co_ensure(co_await install_key(0, KeyCipher::AesCcm, AesKeyLen::Bits128, KeyType::Ptk, supp.ptk.tk, 0), "install ptk failed");
    const auto& gtk = supp.group.gtk;
    co_ensure(co_await install_key(gtk.key_id, KeyCipher::AesCcm,
                                   gtk.len == 32 ? AesKeyLen::Bits256 : AesKeyLen::Bits128, KeyType::Gtk,
                                   {gtk.key, gtk.len}, 0),
              "install gtk failed");
    // the fw does not accept the igtk (INSTALL_KEY returns -EOPNOTSUPP); the
    // reference driver keeps it host-side and validates BIP in software
    // (ref umac_keys_mmdrv_install_key, supplicant_shim/bip.c). protected mgmt
    // rx validation is not implemented yet, so the igtk is dropped here
    if(supp.group.igtk) {
        log<"halow: igtk received (id {}), software bip not implemented\n">((*supp.group.igtk).key_id);
    }

    link.encrypted = true;
    log<"halow: 4-way handshake complete, keys installed\n">();
    co_return true;
}

auto associate(const noxx::StringView ssid, const bool secure) -> coop::Async<bool> {
    constexpr auto error_value = false;

    // ref frame_association_request_build
    auto frame = noxx::Array<u8, 192>();
    auto w     = noxx::SpanWriter(frame);
    co_ensure(w.append_obj(dot11::AssocReq{
        .header          = make_mgmt_header(dot11::Fc::AssocReq, link.bssid, link.mac, link.bssid),
        .capability      = assoc_capability,
        .listen_interval = 0,
    }));
    co_ensure(w.append_obj(connect::ie::Header{
        .id     = connect::ie::Id::Ssid,
        .length = u8(ssid.size()),
    }));
    co_ensure(w.append_span(ssid));
    co_ensure(w.append_obj(connect::ie::Header{
        .id     = connect::ie::Id::AidRequest,
        .length = 1,
    }));
    co_ensure(w.append_obj(u8(0))); // aid mode
    // rsn information element for WPA3-SAE (the element already carries its id/length header)
    if(secure) {
        co_ensure(w.append_obj(rsn_ie));
    }
    // s1g capabilities ie, must match the one sent in probe requests
    co_ensure(w.append_obj(make_s1g_capabilities()));
    // rsn extension element (signals SAE hash-to-element), after the s1g caps
    if(secure) {
        co_ensure(w.append_obj(rsnxe));
    }
    // wmm information element (ref ie_wmm_info_build)
    constexpr auto wmm_info = noxx::to_array<u8>({0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00});
    co_ensure(w.append_obj(connect::ie::Header{
        .id     = connect::ie::Id::VendorSpecific,
        .length = 0,
    }));
    co_ensure(w.append_obj(connect::ie::Header{
        .id     = connect::ie::Id::VendorSpecific,
        .length = u8(sizeof(wmm_info)),
    }));
    co_ensure(w.append_obj(wmm_info));

    co_ensure(co_await tx_mgmt({frame.data, w.buf.data - frame.data}));
    co_unwrap(rx, co_await wait_mgmt(dot11::Fc::AssocResp, response_wait_us), "no association response");
    co_unwrap(skbh, parse_skb_header(*rx));
    auto r = noxx::SpanReader(noxx::Span<const u8>{packet_frame(*rx, skbh), skbh.len});
    co_unwrap(resp, r.read<dot11::AssocResp>(), "short assoc response");
    co_ensure(resp.status_code == dot11::status_success, "association refused");

    // aid comes in the s1g aid response ie
    auto aid = u16(0);
    if(skbh.rx_status.flags & RxFlag::FcsIncluded) {
        co_ensure(r.buf.size >= dot11::fcs_len);
        r.buf.size -= dot11::fcs_len;
    }
    while(r.buf.size > 0 && aid == 0) {
        co_unwrap(ieh, r.read<connect::ie::Header>());
        co_unwrap(body, r.read_span(ieh.length));
        switch(ieh.id) {
        case connect::ie::Id::AidResponse:
            co_ensure(body.size() >= sizeof(aid));
            aid = *(uu16*)body.data;
            break;
        }
    }
    co_ensure(aid != 0, "no aid in assoc response");
    link.aid = aid;
    co_return true;
}

// classify one from-air frame: tear the link down on deauth/disassoc from the
// ap, decode data frames into the rx queue, drop the rest
auto handle_air_frame(net::AutoPacket rx) -> coop::Async<void> {
    const auto skbh = parse_skb_header(*rx);
    if(skbh == nullptr) {
        co_return;
    }

    // peek the 802.11 frame control for a deauth/disassoc from our bss before
    // handing data frames off to eth_from_rx
    const auto body = packet_frame(*rx, *skbh);
    auto       len  = usize(skbh->len);
    if(skbh->rx_status.flags & RxFlag::FcsIncluded) {
        if(len < dot11::fcs_len) {
            co_return;
        }
        len -= dot11::fcs_len;
    }
    if(len >= sizeof(dot11::Header)) {
        const auto wifi = (const dot11::Header*)body;
        const auto kind = wifi->frame_control & dot11::Fc::VerTypeSubMask;
        if((kind == dot11::Fc::Deauth || kind == dot11::Fc::Disassoc) && mac_equal(wifi->addr2.data, link.bssid.data)) {
            log<"halow: ap sent {}, link down\n">(kind == dot11::Fc::Deauth ? "deauth" : "disassoc");
            link.up = false;
            co_return;
        }
    }

    // deliver the decoded ethernet frame straight to the ip stack; frames that
    // arrive before `net up` attaches the stack are dropped here
    if(eth_from_rx(*rx) && netif.stack != nullptr) {
        co_await netif.rx(noxx::move(rx));
    }
}
} // namespace

auto connect(const dot11::MacAddr& mac, const noxx::StringView ssid, const noxx::StringView password) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(!link.up, "already connected");
    co_ensure(ssid.size() > 0 && ssid.size() <= ssid_max_len);
    const auto secure = password.size() > 0;

    // find the ap
    auto results = noxx::Array<ScanResult, 4>();
    co_unwrap(count, co_await scan(mac, ssid, results));
    auto ap = (const ScanResult*)nullptr;
    for(auto i = usize(0); i < count; i += 1) {
        const auto& res = results[i];
        if(res.ssid_len == ssid.size() && noxx::StringView((const char*)res.ssid, res.ssid_len) == ssid) {
            ap = &res;
            break;
        }
    }
    co_ensure(ap != nullptr, "ap not found");
    co_ensure(ap->s1g_op, "ap has no s1g operation ie");
    const auto ap_privacy = bool(ap->capability_info & dot11::CapInfo::Privacy);
    co_ensure(secure || !ap_privacy, "ap requires encryption, pass a password");
    co_ensure(!secure || ap_privacy, "ap is open, do not pass a password");
    log<"halow: connecting to {} rssi {}\n">(ap->bssid, ap->rssi);

    link.mac       = mac;
    link.bssid     = ap->bssid;
    link.encrypted = false;

    // sta interface
    const auto if_req = AddInterfaceReq{
        .addr           = mac,
        .interface_type = InterfaceType::Sta,
    };
    auto vif = vif_invalid;
    co_ensure(co_await send_command(CommandId::AddInterface, as_span(if_req), {}, vif_invalid, &vif));
    co_ensure(vif != vif_invalid);
    link.vif = vif;

    // need to remove interface on failure from here
    do {
#pragma push_macro("error_act")
#define error_act break

        co_ensure(co_await configure_channel(*ap->s1g_op));
        // ref mmdrv_cfg_bss: beacon interval, dtim 0, cssid 0
        const auto req = BssConfigReq{
            .beacon_interval_tu = ap->beacon_interval,
            .dtim_period        = 0,
            .cssid              = 0,
        };
        co_ensure(co_await send_command(CommandId::BssConfig, as_span(req), {}, link.vif));

        // keep the radio awake: chip power save would doze between beacons,
        // dropping data tx and the ap's probes
        const auto ps_req = ConfigPsReq{
            .enabled            = 0,
            .dynamic_ps_offload = 0,
        };
        co_ensure(co_await send_command(CommandId::ConfigPs, as_span(ps_req), {}, link.vif));

        co_ensure(co_await configure_qos());
        co_ensure((secure ? co_await sae_authenticate(ssid, password) : co_await authenticate()));
        co_ensure(co_await set_sta_state(StaState::Authenticated, 0));
        co_ensure(co_await associate(ssid, secure));
        // the 4-way handshake runs over the datapath while still authenticated, then
        // installs the ccmp keys before the port is authorized
        co_ensure((secure ? co_await four_way_handshake() : true));
        co_ensure(co_await set_sta_state(StaState::Authorized, link.aid));

        link.up       = true;
        last_activity = time::now();
        // hand ongoing rx / keepalive / loss detection to a background task,
        // keeping its handle so disconnect can cancel it
        co_ensure((co_await coop::reveal_runner())->push_task(link_task(), &link_handle), "failed to start link task");
        log<"halow: associated, aid {}\n">(link.aid);
        co_return true;

#pragma pop_macro("error_act")
    } while(0);

    log<"halow: connect failed, removing interface\n">();
    co_await send_command(CommandId::RemoveInterface, {}, {}, link.vif);
    link = LinkStatus();
    co_return false;
}

auto disconnect() -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(link.up, "not connected");
    // cancel the maintenance task first so it stops touching the yaps stream
    // while we run the teardown commands below (link.up is true here, so the
    // task is still alive and the handle is valid)
    link_handle.cancel();
    link.up = false;

    const auto deauth = dot11::Deauth{
        .header      = make_mgmt_header(dot11::Fc::Deauth, link.bssid, link.mac, link.bssid),
        .reason_code = dot11::Reason::DeauthLeaving,
    };
    co_await tx_mgmt(as_span(deauth));

    co_await set_sta_state(StaState::None, 0);
    co_await send_command(CommandId::RemoveInterface, {}, {}, link.vif);
    link = LinkStatus();
    co_return true;
}

auto link_desynced() -> bool {
    return rx_desynced();
}

auto send_keepalive() -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(link.up, "not connected");
    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);

    // to-ds qos-null: addr1 = bssid, addr2 = us, addr3 = bssid, no payload
    // (ref umac_datapath_build_3addr_to_ds_qos_null)
    auto w = net::PacketWriter(*packet);
    co_ensure(w.append_obj(dot11::QosData{
        .header      = make_mgmt_header(dot11::Fc::QosNull | dot11::Fc::ToDs, link.bssid, link.mac, link.bssid),
        .qos_control = 0,
    }));

    pkt_id += 1;
    const auto info = SkbHeader::TxInfo{
        .flags  = TxFlag::ImmediateReport | BF(TxFlag::VifId, u32(link.vif)),
        .pkt_id = pkt_id,
        .tid    = mgmt_tid,
        .rates  = {{.rate_code = mgmt_rate(), .count = mgmt_attempts}},
    };
    co_return co_await yaps_tx(SkbChan::Data, *packet, &info);
}

auto link_task() -> coop::Async<bool> {
    constexpr auto error_value = false;

    last_activity = time::now();
    while(link.up) {
        // firmware-signalled loss (beacon loss / tsf reset)
        while(auto event = pop_event()) {
            const auto id = event_id(*event);
            if(id == EventId::BeaconLoss || id == EventId::ConnectionLoss) {
                log<"halow: link lost (event 0x{04x})\n">(id);
                link.up = false;
            }
        }
        // rx_task gave up on a wedged rx stream
        if(rx_desynced()) {
            log<"halow: rx stream desynced, link down (reboot required)\n">();
            link.up = false;
        }
        if(!link.up) {
            break;
        }

        // keep the association fresh while idle
        if(time::now() - last_activity > keepalive_interval_us) {
            if(co_await send_keepalive()) {
                last_activity = time::now();
            }
        }

        // sleep until rx_task queues a frame; the slice bounds how stale the
        // event/desync/keepalive checks above can get
        auto rx = co_await wait_rx(link_wake_us);
        if(rx) {
            last_activity = time::now();
            co_await handle_air_frame(noxx::move(rx));
        }
    }

    // reached only on an involuntary loss — an explicit disconnect cancels the
    // task before it gets here. remove the chip interface so a later connect
    // starts clean, and reset the link state
    co_await send_command(CommandId::RemoveInterface, {}, {}, link.vif);
    link = LinkStatus();
    co_return true;
}

auto link_status() -> const LinkStatus& {
    return link;
}

auto eth_tx(const dot11::MacAddr& dst, const u16 ethertype, const noxx::Span<const u8> payload) -> coop::Async<bool> {
    constexpr auto error_value = false;

    // aid is assigned at association; the 4-way handshake sends eapol frames
    // through here before connect() marks the link up
    co_ensure(link.aid != 0, "not associated");
    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);
    auto w = net::PacketWriter(*packet);

    // qos data to the ds: addr1 = bssid, addr2 = us, addr3 = final destination
    // (ref umac_datapath_construct_80211_data_header_sta). the protected bit is
    // set once the pairwise key is installed; the fw does the ccmp encryption
    const auto fc = dot11::Fc::QosData | dot11::Fc::ToDs | (link.encrypted ? dot11::Fc::Protected : 0);
    co_ensure(w.append_obj(dot11::QosData{
        .header      = make_mgmt_header(fc, link.bssid, link.mac, dst),
        .qos_control = 0, // tid 0
    }));
    co_ensure(w.append_obj(dot11::llc_snap));
    co_ensure(w.append_obj(noxx::byteswap(ethertype)));
    co_ensure(w.append_span(payload), "payload too long");

    // multicast frames are not acked, they ride the no-ack data channel
    // (ref mmdrv_tx_frame)
    const auto multicast = bool(dst[0] & 1);
    pkt_id += 1;
    const auto info = SkbHeader::TxInfo{
        .flags  = TxFlag::ImmediateReport |
                  BF(TxFlag::VifId, u32(link.vif)) |
                  (link.encrypted ? (TxFlag::HwEncrypt | BF(TxFlag::KeyIdx, u32(0))) : 0),
        .pkt_id = pkt_id,
        .tid    = 0,
        .rates  = {{.rate_code = mgmt_rate(), .count = u8(multicast ? 1 : mgmt_attempts)}},
    };
    co_return co_await yaps_tx(multicast ? SkbChan::DataNoAck : SkbChan::Data, *packet, &info);
}

auto eth_from_rx(net::Packet& packet) -> bool {
    constexpr auto error_value = false;

    unwrap(skbh, parse_skb_header(packet));
    auto r = noxx::SpanReader(noxx::Span<const u8>{packet_frame(packet, skbh), skbh.len});
    if(skbh.rx_status.flags & RxFlag::FcsIncluded) {
        ensure(r.buf.size >= dot11::fcs_len);
        r.buf.size -= dot11::fcs_len;
    }
    const auto wifi_p = r.read<dot11::Header>();
    if(wifi_p == nullptr) {
        log<"halow: rx short frame chan 0x{02x} len {}\n">(skbh.channel, skbh.len);
        return false;
    }
    // the ap delivers both qos and plain data frames; drop everything else
    // (s1g beacons arrive continuously, they are skipped silently)
    const auto& wifi    = *wifi_p;
    const auto  fc      = wifi.frame_control;
    const auto  is_qos  = (fc & dot11::Fc::VerTypeSubMask) == dot11::Fc::QosData;
    const auto  is_data = (fc & dot11::Fc::VerTypeSubMask) == dot11::Fc::TypeData;
    if((!is_qos && !is_data) || (fc & dot11::Fc::ToDs)) {
        if((fc & dot11::Fc::VerTypeSubMask) != dot11::Fc::S1gBeacon) {
            log<"halow: rx non-data frame fc 0x{04x} len {}\n">(fc, skbh.len);
        }
        return false;
    }
    if(is_qos) {
        ensure(r.read(sizeof(dot11::QosData) - sizeof(dot11::Header)) != nullptr, "rx short data frame");
    }
    // hardware-decrypted ccmp frames keep the 8-byte ccmp header (right after
    // the mac header) and the 8-byte mic (before the fcs); strip both so the
    // llc/snap lands where the plaintext path expects it. ref umac_datapath
    if(skbh.rx_status.flags & RxFlag::Decrypted) {
        ensure(r.read(dot11::ccmp_hdr_len) != nullptr, "rx short data frame");
        ensure(r.buf.size >= dot11::ccmp_mic_len, "rx short data frame");
        r.buf.size -= dot11::ccmp_mic_len;
    }
    unwrap(llc, r.read_span(sizeof(dot11::llc_snap)), "rx short data frame");
    ensure(llc == dot11::llc_snap, "unexpected llc header");
    ensure(r.buf.size >= sizeof(u16), "rx short data frame"); // ethertype must follow the snap

    // rebuild the ethernet header just before the payload's ethertype (now at
    // the reader position): da = addr1, sa = addr3 (from-ds). sa is copied
    // first, da would clobber its tail
    const auto eth = (net::EthernetHeader*)(r.buf.data - __builtin_offsetof(net::EthernetHeader, ethertype));
    eth->src       = wifi.addr3;
    eth->dst       = wifi.addr1;
    packet.len     = u16((r.buf.data + r.buf.size) - packet.data()); // drop fcs and ccmp mic
    ensure(packet.consume(usize((const u8*)eth - packet.data())));
    return true;
}
} // namespace halow
