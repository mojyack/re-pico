#pragma once
#include <coop/generator.hpp>
#include <crypto/ie.hpp>
#include <halow-regdb.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

#include "dot11.hpp"

// hardware scan for s1g access points (ref umac/scan/hw_scan.c)
namespace halow {
constexpr auto ssid_max_len = usize(32);

struct ScanResult {
    dot11::MacAddr bssid;
    u8             ssid[ssid_max_len];
    u8             ssid_len;
    i16            rssi;
    u16            freq_100khz;
    u16            beacon_interval;
    u16            capability_info;

    noxx::Optional<crypto::ie::S1gOp> s1g_op;
};

// the regdom matching the country the firmware's bcf was loaded with
auto find_regdom() -> const Regdom*;

// the s1g capabilities ie advertised in probe and association requests
// (ref ie_s1g_capabilities_build)
auto make_s1g_capabilities() -> crypto::ie::S1gCaps;

// run a chip-driven scan over the regdom the firmware was loaded with;
// a non-empty ssid narrows the probe to that network.
// fills results, returns the number found
auto scan(const dot11::MacAddr& mac, noxx::StringView ssid, noxx::Span<ScanResult> results) -> coop::Async<noxx::Optional<usize>>;
} // namespace halow
