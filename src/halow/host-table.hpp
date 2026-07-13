#pragma once
#include <noxx/int.hpp>
#include <noxx/optional.hpp>

namespace halow {
// yaps stream interface published by the firmware in the extended host table
// (ref morse_driver/mm8108/yaps-hw.h struct morse_yaps_hw_table)
struct YapsTable {
    u32 ysl_addr;         // from-chip stream: delimiter word, payload at +4
    u32 yds_addr;         // to-chip stream: delimiter word followed by frame
    u32 status_regs_addr; // struct of u32 pool/queue counters
    u16 tc_tx_pool_size;
    u16 fc_rx_pool_size;
    u8  tc_cmd_pool_size;
    u8  tc_beacon_pool_size;
    u8  tc_mgmt_pool_size;
    u8  fc_resp_pool_size;
    u8  fc_tx_sts_pool_size;
    u8  fc_aux_pool_size;
    u8  tc_tx_q_size;
    u8  tc_cmd_q_size;
    u8  tc_beacon_q_size;
    u8  tc_mgmt_q_size;
    u8  fc_q_size;
    u8  fc_done_q_size;
    u16 reserved_page_size;
};

struct HostTable {
    u32       firmware_flags;
    u8        mac[6];
    YapsTable yaps;
};

// read and parse the extended host table published by the booted firmware
// (ref morse_firmware_parse_extended_host_table)
auto parse_host_table(u32 host_table_ptr) -> noxx::Optional<HostTable>;
} // namespace halow
