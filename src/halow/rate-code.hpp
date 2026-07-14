#pragma once
#include <noxx/bits.hpp>
#include <noxx/int.hpp>

// morse rate codes (ref morse_driver/morse_rate_code.h)
namespace halow {
// bandwidth index (enum dot11_bandwidth)
struct RateBw {
    enum : u32 {
        Bw1Mhz  = 0,
        Bw2Mhz  = 1,
        Bw4Mhz  = 2,
        Bw8Mhz  = 3,
        Bw16Mhz = 4,
    };
};

// preamble (enum morse_rate_preamble); the 11b/g preambles are omitted
struct RatePreamble {
    enum : u32 {
        S1gLong  = 0,
        S1gShort = 1,
        S1g1Mhz  = 2,
    };
};

// rate code fields (MORSE_RATECODE_*)
struct RateCode {
    enum : u32 {
        Preamble         = 0x0000'000f, // RatePreamble
        McsIndex         = 0x0000'00f0,
        NssIndex         = 0x0000'0700,
        BwIndex          = 0x0000'3800, // RateBw
        LdpcFlag         = 0x0000'4000,
        StbcFlag         = 0x0000'8000,
        RtsFlag          = 0x0001'0000,
        Cts2SelfFlag     = 0x0002'0000,
        ShortGiFlag      = 0x0004'0000,
        TravPilotsFlag   = 0x0008'0000,
        CtrlResp1MhzFlag = 0x0010'0000,
        DupFormatFlag    = 0x0020'0000,
        DupBwIndex       = 0x01c0'0000, // RateBw
    };
};

// ref morse_ratecode_init
constexpr auto make_rate_code(const u32 bw_index, const u32 nss_index, const u32 mcs_index, const u32 preamble) -> u32 {
    return BF(RateCode::BwIndex, bw_index) |
           BF(RateCode::NssIndex, nss_index) |
           BF(RateCode::McsIndex, mcs_index) |
           BF(RateCode::Preamble, preamble);
}

// s1g rate with the mandatory preamble for the bandwidth
// (ref morse_ratecode_update_s1g_bw_preamble)
constexpr auto make_s1g_rate_code(const u32 bw_index, const u32 nss_index, const u32 mcs_index) -> u32 {
    const auto preamble = bw_index == RateBw::Bw1Mhz ? RatePreamble::S1g1Mhz : RatePreamble::S1gShort;
    return make_rate_code(bw_index, nss_index, mcs_index, preamble);
}
static_assert(make_s1g_rate_code(RateBw::Bw1Mhz, 0, 0) == 2);
static_assert(make_s1g_rate_code(RateBw::Bw2Mhz, 0, 0) == (1 << 11 | 1));
} // namespace halow
