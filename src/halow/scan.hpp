#pragma once
#include <coop/generator.hpp>
#include <halow-regdb.hpp>
#include <noxx/optional.hpp>
#include <noxx/span.hpp>
#include <noxx/string-view.hpp>

#include "dot11.hpp"

// hardware scan for s1g access points (ref umac/scan/hw_scan.c)
namespace halow {
constexpr auto ssid_max_len = usize(32);

struct ScanResult {
    u8  bssid[dot11::mac_len];
    u8  ssid[ssid_max_len];
    u8  ssid_len;
    i16 rssi;
    u16 freq_100khz;
    u16 beacon_interval;
    u16 capability_info;
    u8  s1g_op[dot11::S1gOp::Size]; // s1g operation ie body, valid if has_s1g_op
    bool has_s1g_op;
};

// the regdom matching the country the firmware's bcf was loaded with
auto find_regdom() -> const Regdom*;

// run a chip-driven scan over the regdom the firmware was loaded with;
// a non-empty ssid narrows the probe to that network.
// fills results, returns the number found
auto scan(const u8 (&mac)[dot11::mac_len], noxx::StringView ssid, noxx::Span<ScanResult> results) -> coop::Async<noxx::Optional<usize>>;
} // namespace halow
