#include <coop/promise.hpp>
#include <coop/timer.hpp>
#include <hal/time.hpp>
#include <halow-fw-blob.hpp>
#include <halow-regdb.hpp>
#include <noxx/array.hpp>

#include "command.hpp"
#include "dot11.hpp"
#include "scan.hpp"
#include "util.hpp"
#include "yaps.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// hw scan command payload (ref umac/scan/hw_scan.c, common/morse_commands.h)
struct HwScanFlag {
    enum : u32 {
        Start             = 1 << 0,
        Abort             = 1 << 1,
        Survey            = 1 << 2,
        Store             = 1 << 3,
        Probes1Mhz        = 1 << 4,
        SchedStart        = 1 << 5,
        SchedStop         = 1 << 6,
        ProbeOnDozeBeacon = 1 << 7,
    };
};

struct HwScanTlv {
    enum : u16 {
        Pad         = 0,
        ProbeReq    = 1,
        ChanList    = 2,
        PowerList   = 3,
        DwellOnHome = 4,
        Sched       = 5,
        Filter      = 6,
        SchedParams = 7,
    };
};

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

constexpr u8 broadcast_mac[dot11::mac_len] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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
auto build_probe_request(Builder& b, const u8 (&mac)[dot11::mac_len], const noxx::StringView ssid) -> void {
    // mgmt header, da/bssid broadcast
    b.put_u16(dot11::Fc::ProbeReq); // frame control
    b.put_u16(0);                   // duration
    b.put_bytes(broadcast_mac, dot11::mac_len);
    b.put_bytes(mac, dot11::mac_len);
    b.put_bytes(broadcast_mac, dot11::mac_len);
    b.put_u16(0); // sequence control

    // ssid ie, empty = wildcard
    b.put(dot11::Ie::Ssid);
    b.put(u8(ssid.size()));
    b.put_bytes((const u8*)ssid.data(), ssid.size());

    // s1g capabilities ie: 10 info bytes + 5 mcs/nss bytes
    // (ref ie_s1g_capabilities_build)
    auto caps = noxx::Array<u8, 15>();
    caps[0]   = dot11::S1gCap0::SuppWidth1248Mhz;
    caps[4]   = dot11::S1gCap4::StaTypeNonSensor;
    caps[7]   = dot11::S1gCap7::Dup1MhzSupport;
    // mcs map: 1ss up to mcs 7, 2-4ss unsupported; stored once, then again
    // shifted across bytes 2-3 (rx/tx halves, as the reference builder does)
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

// hw scan request payload: flags, dwell, then chan list / power list / probe request tlvs
auto build_hw_scan_req(Builder& b, const Regdom& regdom, const u8 (&mac)[dot11::mac_len], const noxx::StringView ssid) -> void {
    b.put_u32(HwScanFlag::Start);
    b.put_u32(dwell_time_ms);

    auto powers = PowerList();
    auto len_at = b.begin_tlv(HwScanTlv::ChanList);
    for(auto i = u32(0); i < regdom.num_channels; i += 1) {
        const auto& channel = regdom.channels[i];
        if(channel_scannable(channel)) {
            b.put_u32(pack_channel(channel, powers.index_of(channel.max_tx_eirp_dbm)));
        }
    }
    b.end_tlv(len_at);

    len_at = b.begin_tlv(HwScanTlv::PowerList);
    for(auto i = u32(0); i < powers.count; i += 1) {
        b.put_u32(u32(i32(powers.eirp_dbm[i]) * qdbm_per_dbm));
    }
    b.end_tlv(len_at);

    len_at = b.begin_tlv(HwScanTlv::ProbeReq);
    build_probe_request(b, mac, ssid);
    b.end_tlv(len_at);
}

// record a probe response into results if it is new; returns updated count.
// from-air frames arrive on the data channel regardless of 802.11 type,
// dispatch is on the frame control field
auto process_mgmt_frame(const net::Packet& packet, const noxx::Span<ScanResult> results, const usize count) -> usize {
    auto hdr_o = parse_skb_header(packet);
    if(!hdr_o) {
        return count;
    }
    const auto& hdr = *hdr_o;
    if(hdr.channel != SkbChan::Data && hdr.channel != SkbChan::Mgmt && hdr.channel != SkbChan::Beacon) {
        return count;
    }
    const auto body = packet.data() + skb_hdr_size + hdr.offset;
    auto       len  = usize(hdr.len);
    if(hdr.rx_flags & RxFlag::FcsIncluded) {
        if(len < dot11::fcs_len) {
            return count;
        }
        len -= dot11::fcs_len;
    }
    if(len < dot11::ProbeResp::Ies ||
       (get_u16(body + dot11::Hdr::FrameControl) & dot11::Fc::VerTypeSubMask) != dot11::Fc::ProbeResp) {
        return count;
    }
    const auto bssid = body + dot11::Hdr::Addr3;
    for(auto i = usize(0); i < count; i += 1) {
        if(mac_equal(results[i].bssid, bssid)) {
            return count; // duplicate
        }
    }
    if(count >= results.size) {
        return count;
    }

    auto& res = results[count];
    noxx::memcpy(res.bssid, bssid, dot11::mac_len);
    res.beacon_interval = get_u16(body + dot11::ProbeResp::BeaconInterval);
    res.capability_info = get_u16(body + dot11::ProbeResp::Capability);
    res.rssi            = i16(hdr.rssi);
    res.freq_100khz     = hdr.freq_100khz;
    res.ssid_len        = 0;
    res.has_s1g_op      = false;

    // pick the ssid and s1g operation ies
    auto p   = body + dot11::ProbeResp::Ies;
    auto end = body + len;
    while(p + dot11::ie_hdr_size <= end && p + dot11::ie_hdr_size + p[1] <= end) {
        if(p[0] == dot11::Ie::Ssid) {
            res.ssid_len = p[1] < sizeof(res.ssid) ? p[1] : sizeof(res.ssid);
            noxx::memcpy(res.ssid, p + dot11::ie_hdr_size, res.ssid_len);
        } else if(p[0] == dot11::Ie::S1gOperation && p[1] >= dot11::S1gOp::Size) {
            noxx::memcpy(res.s1g_op, p + dot11::ie_hdr_size, dot11::S1gOp::Size);
            res.has_s1g_op = true;
        }
        p += dot11::ie_hdr_size + p[1];
    }
    return count + 1;
}
} // namespace

// the regdom matching the country the firmware's bcf was loaded with
auto find_regdom() -> const Regdom* {
    for(auto i = u32(0); i < regdoms_count; i += 1) {
        if(regdoms[i].country[0] == fw_country[0] && regdoms[i].country[1] == fw_country[1]) {
            return &regdoms[i];
        }
    }
    return nullptr;
}

auto scan(const u8 (&mac)[dot11::mac_len], const noxx::StringView ssid, const noxx::Span<ScanResult> results) -> coop::Async<noxx::Optional<usize>> {
    constexpr auto error_value = noxx::nullopt;

    const auto regdom = find_regdom();
    co_ensure(regdom != nullptr, "no channel list for fw country");

    // sta interface for the scan (ref struct morse_cmd_req_add_interface)
    auto if_req = noxx::Array<u8, dot11::mac_len + 4>();
    auto if_b   = Builder{{if_req.data, if_req.size()}};
    if_b.put_bytes(mac, dot11::mac_len);
    if_b.put_u32(InterfaceType::Sta);
    auto vif = vif_invalid;
    co_ensure(co_await send_command(CommandId::AddInterface, {if_req.data, if_b.offset}, {nullptr, 0}, vif_invalid, &vif));
    co_ensure(vif != vif_invalid, "no vif in add interface response");
    log<"halow: scan vif {} regdom {}\n">(vif, noxx::StringView(regdom->country, 2));

    while(pop_event()) { // drop stale events
    }

    auto req = noxx::Array<u8, hw_scan_req_max>();
    auto b   = Builder{{req.data, req.size()}};
    build_hw_scan_req(b, *regdom, mac, ssid);
    co_ensure(!b.overflow, "hw scan request too long");
    const auto started = co_await send_command(CommandId::HwScan, {req.data, b.offset}, {nullptr, 0}, vif);

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
            auto abort_req = noxx::Array<u8, 8>();
            auto abort_b   = Builder{{abort_req.data, abort_req.size()}};
            abort_b.put_u32(HwScanFlag::Abort);
            abort_b.put_u32(0); // dwell time, unused
            co_await send_command(CommandId::HwScan, {abort_req.data, abort_b.offset}, {nullptr, 0}, vif);
        }
    }

    co_await send_command(CommandId::RemoveInterface, {nullptr, 0}, {nullptr, 0}, vif);
    co_ensure(started && done, "scan failed");
    co_return count;
}
} // namespace halow
