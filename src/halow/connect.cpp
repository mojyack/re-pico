#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <noxx/array.hpp>

#include "command.hpp"
#include "connect.hpp"
#include "scan.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// sta states for SET_STA_STATE (ref mmdrv.h enum morse_sta_state)
struct StaState {
    enum : u16 {
        NotExist      = 0,
        None          = 1,
        Authenticated = 2,
        Associated    = 3,
        Authorized    = 4,
    };
};

constexpr auto qdbm_per_dbm     = i32(4);
constexpr auto mgmt_tid         = u8(7); // MMWLAN_MAX_QOS_TID
constexpr auto mgmt_attempts    = u8(5);
constexpr auto auth_tries       = u32(3);
constexpr auto response_wait_us = u64(1'000'000);
constexpr auto rx_poll_ms       = u32(5);
constexpr auto assoc_capability = u16(dot11::CapInfo::Privacy | dot11::CapInfo::ShortPreamble |
                                      dot11::CapInfo::Qos | dot11::CapInfo::ShortSlotTime);
constexpr auto ethertype_offset = usize(12); // within an ethernet header
constexpr auto eth_hdr_len      = usize(14);

constexpr u8 broadcast_mac[dot11::mac_len] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

auto link   = LinkStatus();
auto seq    = u16(0); // 802.11 sequence number, shared space
auto pkt_id = u32(0); // tx status correlation

// primary channel width and rate for the current link, from the s1g operation ie
auto prim_bw_mhz = u8(1);

auto mgmt_rate() -> u32 {
    return prim_bw_mhz == 1 ? RateCode::Mcs0Bw1Mhz : RateCode::Mcs0Bw2Mhz;
}

auto put_mgmt_header(Builder& b, const u16 frame_control, const u8* const addr1, const u8* const addr2, const u8* const addr3) -> void {
    seq = (seq + 1) & 0xfff;
    b.put_u16(frame_control);
    b.put_u16(0); // duration
    b.put_bytes(addr1, dot11::mac_len);
    b.put_bytes(addr2, dot11::mac_len);
    b.put_bytes(addr3, dot11::mac_len);
    b.put_u16(seq << 4);
}

// s1g capabilities ie, must match the one sent in probe requests
auto put_s1g_capabilities_ie(Builder& b) -> void {
    auto caps = noxx::Array<u8, 15>();
    for(auto i = usize(0); i < caps.size(); i += 1) {
        caps[i] = 0;
    }
    caps[0]                = dot11::S1gCap0::SuppWidth1248Mhz;
    caps[4]                = dot11::S1gCap4::StaTypeNonSensor;
    caps[7]                = dot11::S1gCap7::Dup1MhzSupport;
    constexpr auto mcs_map = u8(dot11::S1gNssMaxMcs::Mcs7 << 0 |
                                dot11::S1gNssMaxMcs::None << 2 |
                                dot11::S1gNssMaxMcs::None << 4 |
                                dot11::S1gNssMaxMcs::None << 6);
    caps[10]               = mcs_map;
    caps[12]               = u8(mcs_map << 1);
    caps[13]               = u8(mcs_map >> 7);
    b.put(dot11::Ie::S1gCapabilities);
    b.put(caps.size());
    b.put_bytes(caps.data, caps.size());
}

// send a management frame with the fixed low-rate tx parameters
auto tx_mgmt(const noxx::Span<const u8> frame) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);
    const auto body = packet->append(frame.size);
    co_ensure(body != nullptr, "mgmt frame too long");
    noxx::memcpy(body, frame.data, frame.size);

    pkt_id += 1;
    const auto info = TxInfo{
        .flags    = TxFlag::ImmediateReport | tx_flag_vif(link.vif),
        .pkt_id   = pkt_id,
        .tid      = mgmt_tid,
        .rate     = mgmt_rate(),
        .attempts = mgmt_attempts,
    };
    co_return co_await yaps_tx(SkbChan::Mgmt, *packet, &info);
}

// log a tx status report frame (ref struct morse_skb_tx_status)
auto log_tx_status(const net::Packet& packet, const SkbHeader& hdr) -> void {
    const auto body = packet.data() + skb_hdr_size + hdr.offset;
    if(hdr.len < 12) {
        return;
    }
    const auto flags = get_u32(body);
    const auto id    = get_u32(body + 4);
    log<"halow: tx status pkt {} {}\n">(id, (flags & TxFlag::NoAck) ? "no-ack" : "acked");
}

// await a mgmt frame of the given type from our bss, discarding others
auto wait_mgmt(const u16 frame_control, const u64 timeout_us) -> coop::Async<noxx::Optional<net::AutoPacket>> {
    constexpr auto error_value = noxx::nullopt;

    const auto deadline = time::now() + timeout_us;
    while(time::now() < deadline) {
        auto rx_o = co_await fetch_rx();
        if(!rx_o || !*rx_o) {
            co_await coop::sleep_ms(rx_poll_ms);
            continue;
        }
        auto& rx    = *rx_o;
        auto  hdr_o = parse_skb_header(*rx);
        if(!hdr_o) {
            continue;
        }
        const auto& hdr = *hdr_o;
        if(hdr.channel == SkbChan::TxStatus) {
            log_tx_status(*rx, hdr);
            continue;
        }
        const auto body = packet_frame(*rx, hdr);
        if(hdr.len < dot11::Hdr::Size) {
            continue;
        }
        const auto fc = get_u16(body + dot11::Hdr::FrameControl);
        if((fc & dot11::Fc::VerTypeSubMask) != frame_control) {
            log<"halow: connect skipping frame fc 0x{04x}\n">(fc);
            continue;
        }
        if(!mac_equal(body + dot11::Hdr::Addr2, link.bssid)) {
            continue;
        }
        co_return noxx::move(rx);
    }
    co_return noxx::nullopt;
}

auto set_sta_state(const u16 state, const u16 aid) -> coop::Async<bool> {
    constexpr auto error_value = false;

    // ref struct morse_cmd_req_set_sta_state
    auto req = noxx::Array<u8, dot11::mac_len + 9>();
    auto b   = Builder{{req.data, req.size()}};
    b.put_bytes(link.bssid, dot11::mac_len);
    b.put_u16(aid);
    b.put_u16(state);
    b.put(0);     // uapsd queues
    b.put_u32(0); // flags
    co_return bool(co_await send_command(CommandId::SetStaState, {req.data, b.offset}, {nullptr, 0}, link.vif));
}

// SET_CHANNEL + SET_TXPOWER from the ap's s1g operation ie
auto configure_channel(const u8 (&s1g_op)[dot11::S1gOp::Size]) -> coop::Async<bool> {
    constexpr auto error_value = false;

    const auto regdom = find_regdom();
    co_ensure(regdom != nullptr);
    const S1gChannel* op_chan  = nullptr;
    const S1gChannel* pri_chan = nullptr;
    for(auto i = u32(0); i < regdom->num_channels; i += 1) {
        const auto& channel = regdom->channels[i];
        if(channel.chan_num == s1g_op[dot11::S1gOp::OpChanNum]) {
            op_chan = &channel;
        }
        if(channel.chan_num == s1g_op[dot11::S1gOp::PrimChanNum]) {
            pri_chan = &channel;
        }
    }
    co_ensure(op_chan != nullptr && pri_chan != nullptr, "ap channel not in regdom");

    const auto width  = s1g_op[dot11::S1gOp::ChannelWidth];
    const auto op_bw  = u8(((width & dot11::S1gOpWidth::OpWidthMask) >> 1) + 1);
    const auto pri_bw = u8((width & dot11::S1gOpWidth::PrimIs1Mhz) ? 1 : 2);
    co_ensure(op_bw == op_chan->bw_mhz, "op width mismatch with regdom");

    // index of the primary 1MHz subchannel within the operating channel
    // (ref umac_interface_calc_pri_1mhz_idx)
    auto pri_idx = (i32(pri_chan->freq_khz) - i32(op_chan->freq_khz) + i32(op_bw - pri_bw) * 500) / 1000;
    if(pri_bw == 2 && (width & dot11::S1gOpWidth::PrimLoc)) {
        pri_idx += 1;
    }
    co_ensure(pri_idx >= 0 && pri_idx < op_bw, "invalid primary channel index");

    log<"halow: channel {}khz bw {}MHz primary {}MHz idx {}\n">(op_chan->freq_khz, op_bw, pri_bw, u32(pri_idx));

    // ref struct morse_cmd_req_set_channel
    auto req = noxx::Array<u8, 10>();
    auto b   = Builder{{req.data, req.size()}};
    b.put_u32(op_chan->freq_khz * 1000);
    b.put(op_bw);
    b.put(pri_bw);
    b.put(u8(pri_idx));
    b.put(0); // dot11 mode: s1g
    b.put(0); // deprecated
    b.put(0); // is_off_channel
    co_ensure(co_await send_command(CommandId::SetChannel, {req.data, b.offset}, {nullptr, 0}));

    auto pwr_req = noxx::Array<u8, 4>();
    put_u32(pwr_req.data, u32(i32(op_chan->max_tx_eirp_dbm) * qdbm_per_dbm));
    co_ensure(co_await send_command(CommandId::SetTxPower, {pwr_req.data, pwr_req.size()}, {nullptr, 0}));

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
    constexpr QosParams defaults[] = {
        {3, 15, 1023}, // best effort
        {7, 15, 1023}, // background
        {2, 7, 15},    // video
        {2, 3, 7},     // voice
    };
    constexpr auto txop_max_us = u32(15008);

    for(auto aci = u32(0); aci < 4; aci += 1) {
        // ref struct morse_cmd_req_set_qos_params
        auto req = noxx::Array<u8, 11>();
        auto b   = Builder{{req.data, req.size()}};
        b.put(0); // uapsd
        b.put(u8(aci));
        b.put(defaults[aci].aifs);
        b.put_u16(defaults[aci].cw_min);
        b.put_u16(defaults[aci].cw_max);
        b.put_u32(txop_max_us);
        co_ensure(co_await send_command(CommandId::SetQosParams, {req.data, b.offset}, {nullptr, 0}));
    }
    co_return true;
}

auto authenticate() -> coop::Async<bool> {
    constexpr auto error_value = false;

    // open system authentication, sequence 1 (ref frame_authentication_build)
    auto frame = noxx::Array<u8, dot11::Auth::Size>();
    auto b     = Builder{{frame.data, frame.size()}};
    put_mgmt_header(b, dot11::Fc::Auth, link.bssid, link.mac, link.bssid);
    b.put_u16(dot11::AuthAlg::Open);
    b.put_u16(1); // sequence
    b.put_u16(dot11::status_success);

    for(auto attempt = u32(0); attempt < auth_tries; attempt += 1) {
        co_ensure(co_await tx_mgmt({frame.data, b.offset}));
        auto rx_o = co_await wait_mgmt(dot11::Fc::Auth, response_wait_us);
        if(!rx_o) {
            log<"halow: auth response timeout, retrying\n">();
            continue;
        }
        auto hdr_o = parse_skb_header(**rx_o);
        co_ensure(hdr_o);
        const auto body = packet_frame(**rx_o, *hdr_o);
        co_ensure((*hdr_o).len >= dot11::Auth::Size, "short auth response");
        const auto alg    = get_u16(body + dot11::Auth::Alg);
        const auto rseq   = get_u16(body + dot11::Auth::Seq);
        const auto status = get_u16(body + dot11::Auth::Status);
        co_ensure(alg == dot11::AuthAlg::Open && rseq == 2, "unexpected auth response");
        co_ensure(status == dot11::status_success, "authentication refused");
        co_return true;
    }
    co_ensure(false, "authentication timed out");
}

auto associate(const noxx::StringView ssid) -> coop::Async<bool> {
    constexpr auto error_value = false;

    // ref frame_association_request_build
    auto frame = noxx::Array<u8, 128>();
    auto b     = Builder{{frame.data, frame.size()}};
    put_mgmt_header(b, dot11::Fc::AssocReq, link.bssid, link.mac, link.bssid);
    b.put_u16(assoc_capability);
    b.put_u16(0); // listen interval
    b.put(dot11::Ie::Ssid);
    b.put(u8(ssid.size()));
    b.put_bytes((const u8*)ssid.data(), ssid.size());
    b.put(dot11::Ie::AidRequest);
    b.put(1);
    b.put(0); // aid request mode
    put_s1g_capabilities_ie(b);
    // wmm information element (ref ie_wmm_info_build)
    constexpr u8 wmm_info[] = {0x00, 0x50, 0xf2, 0x02, 0x00, 0x01, 0x00};
    b.put(dot11::Ie::VendorSpecific);
    b.put(sizeof(wmm_info));
    b.put_bytes(wmm_info, sizeof(wmm_info));
    co_ensure(!b.overflow);

    co_ensure(co_await tx_mgmt({frame.data, b.offset}));
    co_unwrap(rx, co_await wait_mgmt(dot11::Fc::AssocResp, response_wait_us), "no association response");
    co_unwrap(hdr, parse_skb_header(*rx));
    const auto body = packet_frame(*rx, hdr);
    co_ensure(hdr.len >= dot11::AssocResp::Ies, "short assoc response");
    const auto status = get_u16(body + dot11::AssocResp::Status);
    co_ensure(status == dot11::status_success, "association refused");

    // aid comes in the s1g aid response ie
    auto aid = u16(0);
    auto len = usize(hdr.len);
    if(hdr.rx_flags & RxFlag::FcsIncluded) {
        len -= dot11::fcs_len;
    }
    auto p   = body + dot11::AssocResp::Ies;
    auto end = body + len;
    while(p + dot11::ie_hdr_size <= end && p + dot11::ie_hdr_size + p[1] <= end) {
        if(p[0] == dot11::Ie::AidResponse && p[1] >= 2) {
            aid = get_u16(p + dot11::ie_hdr_size);
            break;
        }
        p += dot11::ie_hdr_size + p[1];
    }
    co_ensure(aid != 0, "no aid in assoc response");
    link.aid = aid;
    co_return true;
}
} // namespace

auto connect(const u8 (&mac)[dot11::mac_len], const noxx::StringView ssid) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(!link.up, "already connected");
    co_ensure(ssid.size() > 0 && ssid.size() <= ssid_max_len);

    // find the ap
    auto results = noxx::Array<ScanResult, 4>();
    co_unwrap(count, co_await scan(mac, ssid, results));
    const ScanResult* ap = nullptr;
    for(auto i = usize(0); i < count; i += 1) {
        const auto& res = results[i];
        if(res.ssid_len == ssid.size() && noxx::StringView((const char*)res.ssid, res.ssid_len) == ssid) {
            ap = &res;
            break;
        }
    }
    co_ensure(ap != nullptr, "ap not found");
    co_ensure(ap->has_s1g_op, "ap has no s1g operation ie");
    co_ensure(!(ap->capability_info & dot11::CapInfo::Privacy), "ap requires encryption, only open auth supported");
    log<"halow: connecting to {02x}:{02x}:{02x}:{02x}:{02x}:{02x} rssi {}\n">(
        ap->bssid[0], ap->bssid[1], ap->bssid[2], ap->bssid[3], ap->bssid[4], ap->bssid[5], ap->rssi);

    noxx::memcpy(link.mac, mac, dot11::mac_len);
    noxx::memcpy(link.bssid, ap->bssid, dot11::mac_len);

    // sta interface
    auto if_req = noxx::Array<u8, dot11::mac_len + 4>();
    auto if_b   = Builder{{if_req.data, if_req.size()}};
    if_b.put_bytes(mac, dot11::mac_len);
    if_b.put_u32(InterfaceType::Sta);
    auto vif = vif_invalid;
    co_ensure(co_await send_command(CommandId::AddInterface, {if_req.data, if_b.offset}, {nullptr, 0}, vif_invalid, &vif));
    co_ensure(vif != vif_invalid);
    link.vif = vif;

    auto ok = co_await configure_channel(ap->s1g_op);

    // ref mmdrv_cfg_bss: beacon interval, dtim 0, cssid 0
    if(ok) {
        auto req = noxx::Array<u8, 10>();
        auto b   = Builder{{req.data, req.size()}};
        b.put_u16(ap->beacon_interval);
        b.put_u16(0); // dtim period
        b.put_u16(0); // padding
        b.put_u32(0); // cssid
        ok = bool(co_await send_command(CommandId::BssConfig, {req.data, b.offset}, {nullptr, 0}, link.vif));
    }

    // keep the radio awake: chip power save would doze between beacons,
    // dropping data tx and the ap's probes (ref struct morse_cmd_req_config_ps)
    if(ok) {
        constexpr u8 ps_req[] = {0, 0}; // enabled = 0, dynamic_ps_offload = 0
        ok                    = bool(co_await send_command(CommandId::ConfigPs, {ps_req, sizeof(ps_req)}, {nullptr, 0}, link.vif));
    }

    ok = ok && co_await configure_qos();
    ok = ok && co_await authenticate();
    ok = ok && co_await set_sta_state(StaState::Authenticated, 0);
    ok = ok && co_await associate(ssid);
    ok = ok && co_await set_sta_state(StaState::Authorized, link.aid);

    if(!ok) {
        log<"halow: connect failed, removing interface\n">();
        co_await send_command(CommandId::RemoveInterface, {nullptr, 0}, {nullptr, 0}, link.vif);
        link = LinkStatus();
        co_return false;
    }

    link.up = true;
    log<"halow: associated, aid {}\n">(link.aid);
    co_return true;
}

auto disconnect() -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(link.up, "not connected");
    link.up = false;

    // deauthentication frame: reason code 3, deauth because leaving
    auto frame = noxx::Array<u8, dot11::Hdr::Size + 2>();
    auto b     = Builder{{frame.data, frame.size()}};
    put_mgmt_header(b, dot11::Fc::Deauth, link.bssid, link.mac, link.bssid);
    b.put_u16(3);
    co_await tx_mgmt({frame.data, b.offset});

    co_await set_sta_state(StaState::None, 0);
    co_await send_command(CommandId::RemoveInterface, {nullptr, 0}, {nullptr, 0}, link.vif);
    link = LinkStatus();
    co_return true;
}

auto link_status() -> const LinkStatus& {
    return link;
}

auto eth_tx(const u8* const dst, const u16 ethertype, const noxx::Span<const u8> payload) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_ensure(link.up, "not connected");
    const auto packet = net::AutoPacket(net::packet_alloc(tx_headroom));
    co_ensure(packet.get() != nullptr);

    // qos data to the ds: addr1 = bssid, addr2 = us, addr3 = final destination
    // (ref umac_datapath_construct_80211_data_header_sta)
    const auto hdr = packet->append(dot11::QosData::Size + dot11::llc_snap_len);
    co_ensure(hdr != nullptr);
    auto b = Builder{{hdr, dot11::QosData::Size + dot11::llc_snap_len}};
    put_mgmt_header(b, dot11::Fc::QosData | dot11::Fc::ToDs, link.bssid, link.mac, dst);
    b.put_u16(0); // qos control: tid 0
    b.put_bytes(dot11::llc_snap, sizeof(dot11::llc_snap));
    b.put(u8(ethertype >> 8)); // ethertype is big-endian on the wire
    b.put(u8(ethertype));

    const auto body = packet->append(payload.size);
    co_ensure(body != nullptr, "payload too long");
    noxx::memcpy(body, payload.data, payload.size);

    // multicast frames are not acked, they ride the no-ack data channel
    // (ref mmdrv_tx_frame)
    const auto multicast = bool(dst[0] & 1);
    pkt_id += 1;
    const auto info = TxInfo{
        .flags    = TxFlag::ImmediateReport | tx_flag_vif(link.vif),
        .pkt_id   = pkt_id,
        .tid      = 0,
        .rate     = mgmt_rate(),
        .attempts = u8(multicast ? 1 : mgmt_attempts),
    };
    co_return co_await yaps_tx(multicast ? SkbChan::DataNoAck : SkbChan::Data, *packet, &info);
}

auto eth_from_rx(net::Packet& packet) -> bool {
    constexpr auto error_value = false;

    unwrap(hdr, parse_skb_header(packet));
    if(hdr.channel == SkbChan::TxStatus) {
        log_tx_status(packet, hdr);
        return false;
    }
    const auto body = packet.data() + skb_hdr_size + hdr.offset;
    auto       len  = usize(hdr.len);
    if(hdr.rx_flags & RxFlag::FcsIncluded) {
        ensure(len >= dot11::fcs_len);
        len -= dot11::fcs_len;
    }
    if(len < dot11::Hdr::Size) {
        log<"halow: rx short frame chan 0x{02x} len {}\n">(hdr.channel, hdr.len);
        return false;
    }
    // the ap delivers both qos and plain data frames; drop everything else
    // (s1g beacons arrive continuously, they are skipped silently)
    const auto fc      = get_u16(body + dot11::Hdr::FrameControl);
    const auto is_qos  = (fc & dot11::Fc::VerTypeSubMask) == dot11::Fc::QosData;
    const auto is_data = (fc & dot11::Fc::VerTypeSubMask) == dot11::Fc::TypeData;
    if((!is_qos && !is_data) || (fc & dot11::Fc::ToDs)) {
        if((fc & dot11::Fc::VerTypeSubMask) != dot11::Fc::S1gBeacon) {
            log<"halow: rx non-data frame fc 0x{04x} len {}\n">(fc, hdr.len);
        }
        return false;
    }
    const auto hdr_len = is_qos ? u32(dot11::QosData::Size) : u32(dot11::Hdr::Size);
    if(len < hdr_len + dot11::llc_snap_len) {
        log<"halow: rx short data frame len {}\n">(hdr.len);
        return false;
    }
    const auto llc = body + hdr_len;
    for(auto i = usize(0); i < sizeof(dot11::llc_snap); i += 1) {
        ensure(llc[i] == dot11::llc_snap[i], "unexpected llc header");
    }

    // rebuild the ethernet header just before the payload's ethertype:
    // da = addr1, sa = addr3 (from-ds), ethertype already in place after the
    // snap. sa is copied first, da would clobber its tail
    const auto eth = body + hdr_len + sizeof(dot11::llc_snap) - ethertype_offset;
    noxx::memcpy(eth + dot11::mac_len, body + dot11::Hdr::Addr3, dot11::mac_len);
    noxx::memcpy(eth + 0, body + dot11::Hdr::Addr1, dot11::mac_len);
    packet.len = u16((body - packet.data()) + len); // drop fcs
    ensure(packet.consume(usize(eth - packet.data())));
    return true;
}
} // namespace halow
