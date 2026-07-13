#include <noxx/unique-ptr.hpp>

#include "halow.hpp"
#include "host-table.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// host_table field offsets (ref morse_driver/hw.h struct host_table)
constexpr auto tbl_firmware_flags = u32(12);
constexpr auto tbl_ext_table_ptr  = u32(24);

// extended_host_table layout (ref morse_driver/ext_host_table.h): total length,
// mac address, then tlvs whose length field includes the 4-byte tlv header
constexpr auto ext_mac_off  = u32(4);
constexpr auto ext_tlvs_off = u32(10);
constexpr auto ext_max_len  = u32(1024); // sanity bound

constexpr auto tag_yaps_table = u16(3); // MORSE_FW_HOST_TABLE_TAG_YAPS_TABLE
constexpr auto yaps_tlv_size  = u32(36);

auto get_u16(const u8* const p) -> u16 {
    return u16(p[0]) | u16(p[1]) << 8;
}

auto get_u32(const u8* const p) -> u32 {
    return u32(p[0]) | u32(p[1]) << 8 | u32(p[2]) << 16 | u32(p[3]) << 24;
}

// tlv payload mirrors struct morse_yaps_hw_table (flags u8 + padding, then fields)
auto parse_yaps_table(const u8* const p, YapsTable& yaps) -> void {
    yaps.ysl_addr            = get_u32(p + 4);
    yaps.yds_addr            = get_u32(p + 8);
    yaps.status_regs_addr    = get_u32(p + 12);
    yaps.tc_tx_pool_size     = get_u16(p + 16);
    yaps.fc_rx_pool_size     = get_u16(p + 18);
    yaps.tc_cmd_pool_size    = p[20];
    yaps.tc_beacon_pool_size = p[21];
    yaps.tc_mgmt_pool_size   = p[22];
    yaps.fc_resp_pool_size   = p[23];
    yaps.fc_tx_sts_pool_size = p[24];
    yaps.fc_aux_pool_size    = p[25];
    yaps.tc_tx_q_size        = p[26];
    yaps.tc_cmd_q_size       = p[27];
    yaps.tc_beacon_q_size    = p[28];
    yaps.tc_mgmt_q_size      = p[29];
    yaps.fc_q_size           = p[30];
    yaps.fc_done_q_size      = p[31];
    yaps.reserved_page_size  = get_u16(p + 32);
}
} // namespace

auto parse_host_table(const u32 host_table_ptr) -> noxx::Optional<HostTable> {
    constexpr auto error_value = noxx::nullopt;

    auto table = HostTable();
    unwrap(fw_flags, read_u32(host_table_ptr + tbl_firmware_flags));
    table.firmware_flags = fw_flags;

    unwrap(ext_ptr, read_u32(host_table_ptr + tbl_ext_table_ptr));
    ensure(ext_ptr != 0, "no extended host table");
    unwrap(len, read_u32(ext_ptr));
    ensure(len >= ext_tlvs_off && len <= ext_max_len, "bad extended host table length");

    const auto buf_len = (len + 3) & ~u32(3);
    const auto buf     = noxx::make_unique_array<u8>(buf_len);
    ensure(buf);
    ensure(read_multi(ext_ptr, buf.get(), buf_len));
    noxx::memcpy(table.mac, buf.get() + ext_mac_off, sizeof(table.mac));

    auto found_yaps = false;
    auto head       = ext_tlvs_off;
    while(head + 4 <= len) {
        const auto tag     = get_u16(buf.get() + head);
        const auto tlv_len = u32(get_u16(buf.get() + head + 2));
        ensure(tlv_len >= 4 && head + tlv_len <= len, "malformed tlv");
        if(tag == tag_yaps_table && tlv_len >= 4 + yaps_tlv_size) {
            parse_yaps_table(buf.get() + head + 4, table.yaps);
            found_yaps = true;
        }
        head += tlv_len;
    }
    ensure(found_yaps, "no yaps table tlv");
    return table;
}
} // namespace halow
