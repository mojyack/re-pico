#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <crypto/ie.hpp>
#include <hal/time.hpp>
#include <halow-fw-blob.hpp>
#include <halow-regdb.hpp>
#include <noxx/algorithm.hpp>
#include <noxx/array.hpp>
#include <noxx/buf-reader.hpp>
#include <noxx/buf-writer.hpp>

#include "command.hpp"
#include "dot11.hpp"
#include "scan.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// chan list tlv entry bit layout (ref hw_scan.c enum hw_scan_channel)
struct HwScanCh {
    enum : u32 {
        FreqKhzMask    = 0xfffff,
        OpBwShift      = 20, // enum hw_scan_operating_bw
        PrimWidthShift = 22, // enum hw_scan_primary_bw
        PrimIdxShift   = 23,
        PwrIdxShift    = 26,
    };
};

// operating bandwidth values for HwScanCh::OpBwShift
struct HwScanOpBw {
    enum : u32 {
        Bw1Mhz = 0,
        Bw2Mhz = 1,
        Bw4Mhz = 2,
        Bw8Mhz = 3,
    };
};

// primary channel width values for HwScanCh::PrimWidthShift
struct HwScanPrimBw {
    enum : u32 {
        Bw1Mhz = 0,
        Bw2Mhz = 1,
    };
};

constexpr auto qdbm_per_dbm    = i32(4);
constexpr auto dwell_time_ms   = u32(105);
constexpr auto scan_timeout_us = u64(30'000'000);
constexpr auto rx_poll_ms      = u32(5);
constexpr auto hw_scan_req_max = usize(320); // fits the largest regdom chan list

constexpr auto broadcast_mac = dot11::MacAddr{0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

// only 1/2MHz channels are probed (ref hw_scan_construct_channel_list_tlv)
auto channel_scannable(const S1gChannel& channel) -> bool {
    return channel.bw_mhz <= 2;
}

// distinct per-channel tx powers, in first-seen order; a channel's power
// list index is the position of its eirp here (ref hw_scan power indexing)
struct PowerList {
    noxx::Array<i8, 8> eirp_dbm;
    u32                count = 0;

    auto index_of(const i8 eirp) -> u32 {
        for(auto i = u32(0); i < count; i += 1) {
            if(eirp_dbm[i] == eirp) {
                return i;
            }
        }
        if(count < eirp_dbm.size()) {
            eirp_dbm[count] = eirp;
            count += 1;
        }
        return count - 1;
    }
};

// packed channel word for the chan list tlv (ref hw_scan_pack_channel)
auto pack_channel(const S1gChannel& channel, const u32 pwr_idx) -> u32 {
    const auto op_bw      = channel.bw_mhz == 2 ? HwScanOpBw::Bw2Mhz : HwScanOpBw::Bw1Mhz; // scannable channels only
    const auto prim_width = channel.bw_mhz > 1 ? HwScanPrimBw::Bw2Mhz : HwScanPrimBw::Bw1Mhz;
    return (channel.freq_khz & HwScanCh::FreqKhzMask) |
           op_bw << HwScanCh::OpBwShift |
           prim_width << HwScanCh::PrimWidthShift |
           pwr_idx << HwScanCh::PwrIdxShift; // primary channel index 0
}

// probe request frame: pv0 mgmt header + ssid ie + s1g capabilities ie
// (ref umac/frames/probe_request.c)
auto build_probe_request(noxx::BufWriter& w, const dot11::MacAddr& mac, const noxx::StringView ssid) -> bool {
    constexpr auto error_value = false;

    // mgmt header, da/bssid broadcast
    ensure(w.append_obj(dot11::Header{
        .frame_control = dot11::Fc::ProbeReq,
        .addr1         = broadcast_mac,
        .addr2         = mac,
        .addr3         = broadcast_mac,
    }));

    // ssid ie, empty = wildcard
    ensure(w.append_obj(crypto::ie::Header{
        .id     = crypto::ie::Id::Ssid,
        .length = u8(ssid.size()),
    }));
    ensure(w.append_span({(const u8*)ssid.data(), ssid.size()}));
    ensure(w.append_obj(make_s1g_capabilities()));
    return true;
}

// hw scan request payload: flags, dwell, then chan list / power list / probe request tlvs
auto build_hw_scan_req(noxx::BufWriter& w, const Regdom& regdom, const dot11::MacAddr& mac, const noxx::StringView ssid) -> bool {
    constexpr auto error_value = false;

    ensure(w.append_obj(HwScanReq{
        .flags         = HwScanFlag::Start,
        .dwell_time_ms = dwell_time_ms,
    }));

    auto powers = PowerList();
    unwrap(chan_tlv, w.alloc_obj<HwScanTlvHeader>());
    chan_tlv = {.tag = HwScanTlvTag::ChanList};
    for(auto i = u32(0); i < regdom.num_channels; i += 1) {
        const auto& channel = regdom.channels[i];
        if(channel_scannable(channel)) {
            const auto word = pack_channel(channel, powers.index_of(channel.max_tx_eirp_dbm));
            ensure(w.append_obj(word));
            chan_tlv.len += sizeof(word);
        }
    }

    unwrap(power_tlv, w.alloc_obj<HwScanTlvHeader>());
    power_tlv = {.tag = HwScanTlvTag::PowerList};
    for(auto i = u32(0); i < powers.count; i += 1) {
        const auto qdbm = u32(i32(powers.eirp_dbm[i]) * qdbm_per_dbm);
        ensure(w.append_obj(qdbm));
        power_tlv.len += sizeof(qdbm);
    }

    unwrap(probe_tlv, w.alloc_obj<HwScanTlvHeader>());
    probe_tlv = {.tag = HwScanTlvTag::ProbeReq};

    const auto data_before = w.data;
    ensure(build_probe_request(w, mac, ssid));
    probe_tlv.len = u16(w.data - data_before);
    return true;
}

// record a probe response into results if it is new; returns updated count.
// from-air frames arrive on the data channel regardless of 802.11 type,
// dispatch is on the frame control field
auto process_mgmt_frame(const net::Packet& packet, const noxx::Span<ScanResult> results, const usize count) -> usize {
    const auto error_value = count;

    unwrap(skbh, parse_skb_header(packet));
    if(skbh.channel != SkbChan::Data && skbh.channel != SkbChan::Mgmt && skbh.channel != SkbChan::Beacon) {
        return count;
    }
    auto r = noxx::BufReader{packet_frame(packet, skbh), skbh.len};
    if(skbh.rx_status.flags & RxFlag::FcsIncluded) {
        ensure(r.size >= dot11::fcs_len);
        r.size -= dot11::fcs_len;
    }
    unwrap(resp, r.read<dot11::ProbeResponse>());
    if((resp.header.frame_control & dot11::Fc::VerTypeSubMask) != dot11::Fc::ProbeResp) {
        return count;
    }
    for(auto i = usize(0); i < count; i += 1) {
        if(mac_equal(results[i].bssid.data, resp.header.addr3.data)) {
            return count; // duplicate
        }
    }
    if(count >= results.size()) {
        return count;
    }

    auto& res           = results[count];
    res.bssid           = resp.header.addr3;
    res.beacon_interval = resp.beacon_interval;
    res.capability_info = resp.capability_info;
    res.rssi            = i16(skbh.rx_status.rssi);
    res.freq_100khz     = skbh.rx_status.freq_100khz;
    res.ssid_len        = 0;

    // pick the ssid and s1g operation ies
    while(r.size > 0) {
        unwrap(ieh, r.read<crypto::ie::Header>());
        unwrap(body, r.read(ieh.length));
        switch(ieh.id) {
        case crypto::ie::Id::Ssid:
            res.ssid_len = noxx::min<usize>(ieh.length, sizeof(res.ssid));
            noxx::memcpy(res.ssid, &body, res.ssid_len);
            break;
        case crypto::ie::Id::S1gOperation:
            ensure(sizeof(ieh) + ieh.length >= sizeof(crypto::ie::S1gOp));
            res.s1g_op.emplace(*(const crypto::ie::S1gOp*)&ieh);
            break;
        }
    }
    return count + 1;
}
} // namespace

auto make_s1g_capabilities() -> crypto::ie::S1gCaps {
    auto caps = crypto::ie::S1gCaps{
        .header = {
            .id     = crypto::ie::Id::S1gCapabilities,
            .length = sizeof(crypto::ie::S1gCaps) - sizeof(crypto::ie::Header),
        },
    };
    caps.s1g_capabilities_info[0] = crypto::ie::S1gCaps::Cap0::SuppWidth1248Mhz;
    caps.s1g_capabilities_info[4] = crypto::ie::S1gCaps::Cap4::StaTypeNonSensor;
    caps.s1g_capabilities_info[7] = crypto::ie::S1gCaps::Cap7::Dup1MhzSupport;
    // mcs map: 1ss up to mcs 7, 2-4ss unsupported; stored once, then again
    // shifted across bytes 2-3 (rx/tx halves, as the reference builder does)
    constexpr auto mcs_map = u8(crypto::ie::S1gNssMaxMcs::Mcs7 << 0 |
                                crypto::ie::S1gNssMaxMcs::None << 2 |
                                crypto::ie::S1gNssMaxMcs::None << 4 |
                                crypto::ie::S1gNssMaxMcs::None << 6);

    caps.supported_s1g_mcs_and_nss_set[0] = mcs_map;
    caps.supported_s1g_mcs_and_nss_set[2] = u8(mcs_map << 1);
    caps.supported_s1g_mcs_and_nss_set[3] = u8(mcs_map >> 7);
    return caps;
}

// the regdom matching the country the firmware's bcf was loaded with
auto find_regdom() -> const Regdom* {
    for(auto i = u32(0); i < regdoms_count; i += 1) {
        if(regdoms[i].country[0] == fw_country[0] && regdoms[i].country[1] == fw_country[1]) {
            return &regdoms[i];
        }
    }
    return nullptr;
}

auto scan(const dot11::MacAddr& mac, const noxx::StringView ssid, const noxx::Span<ScanResult> results) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    const auto regdom = find_regdom();
    co_ensure(regdom != nullptr, "no channel list for fw country");

    // sta interface for the scan
    const auto if_req = AddInterfaceReq{
        .addr           = mac,
        .interface_type = InterfaceType::Sta,
    };
    auto vif = vif_invalid;
    co_ensure(co_await send_command(CommandId::AddInterface, as_span(if_req), {}, vif_invalid, &vif));
    co_ensure(vif != vif_invalid, "no vif in add interface response");
    log<"halow: scan vif {} regdom {}\n">(vif, noxx::StringView(regdom->country, 2));

    while(pop_event()) { // drop stale events
    }

    auto req = noxx::Array<u8, hw_scan_req_max>();
    auto w   = noxx::BufWriter::from_span(req);
    co_ensure(build_hw_scan_req(w, *regdom, mac, ssid), "hw scan request too long");
    const auto started = co_await send_command(CommandId::HwScan, noxx::Span<const u8>{req.data, w.data - req.data}, {}, vif);

    // collect probe responses until the scan done event
    auto count = usize(0);
    auto done  = false;
    if(started) {
        const auto begun    = time::now();
        const auto deadline = begun + scan_timeout_us;
        while(!done && time::now() < deadline) {
            auto idle = true;
            if(auto rx_o = co_await fetch_rx(); rx_o && *rx_o) {
                count = process_mgmt_frame(**rx_o, results, count);
                idle  = false;
            }
            while(auto event = pop_event()) {
                if(event_id(*event) == EventId::HwScanDone) {
                    done = true;
                    log<"halow: scan done after {}ms, aborted={}\n">(u32((time::now() - begun) / 1000), event_arg(*event));
                }
            }
            if(idle && !done) {
                co_await coop::sleep_ms(rx_poll_ms);
            }
        }
        if(!done) {
            log<"halow: scan timeout, aborting\n">();
            const auto abort_req = HwScanReq{.flags = HwScanFlag::Abort};
            co_await send_command(CommandId::HwScan, as_span(abort_req), {}, vif);
        }
    }

    co_await send_command(CommandId::RemoveInterface, {}, {}, vif);
    co_ensure(started && done, "scan failed");
    co_return count;
}
} // namespace halow
