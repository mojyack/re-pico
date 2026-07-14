#include <noxx/unique-ptr.hpp>

#include "halow.hpp"
#include "host-table.hpp"
#include "util.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// leading host_table fields (ref morse_driver/hw.h struct host_table)
struct HostTableHeader {
    u32 magic_number;
    u32 fw_version_number;
    u32 host_flags;
    u32 firmware_flags; // FwFlag
    u32 memcmd_cmd_addr;
    u32 memcmd_resp_addr;
    u32 extended_host_table_addr;
} __attribute__((packed));

// extended host table (ref morse_driver/ext_host_table.h)

// enum morse_fw_extended_host_table_tag
struct ExtTableTag {
    enum : u16 {
        S1gCapabilities     = 0,
        PagerBypassTxStatus = 1,
        InsertSkbChecksum   = 2,
        YapsTable           = 3,
        PagerPktMemory      = 4,
        PagerBypassCmdResp  = 5,
    };
};

// struct extended_host_table_tlv_hdr; the length includes this header
struct ExtTableTlvHeader {
    u16 tag; // ExtTableTag
    u16 length;
} __attribute__((packed));

// struct extended_host_table; tlvs follow
struct ExtTableHeader {
    u32 extended_host_table_length;
    u8  dev_mac_addr[6];
} __attribute__((packed));

constexpr auto ext_max_len = u32(1024); // sanity bound
} // namespace

auto parse_host_table(const u32 host_table_ptr) -> noxx::Optional<HostTable> {
    constexpr auto error_value = noxx::nullopt;

    auto table  = HostTable();
    auto header = HostTableHeader();
    ensure(read_multi(host_table_ptr, (u8*)&header, sizeof(header)));
    table.firmware_flags = header.firmware_flags;

    const auto ext_ptr = header.extended_host_table_addr;
    ensure(ext_ptr != 0, "no extended host table");
    unwrap(len, read_u32(ext_ptr));
    ensure(len >= sizeof(ExtTableHeader) && len <= ext_max_len, "bad extended host table length");

    const auto buf_len = (len + 3) & ~u32(3);
    const auto buf     = noxx::make_unique_array<u8>(buf_len);
    ensure(buf);
    ensure(read_multi(ext_ptr, buf.get(), buf_len));
    const auto ext = (const ExtTableHeader*)buf.get();
    noxx::memcpy(table.mac.data, ext->dev_mac_addr, sizeof(ext->dev_mac_addr));

    auto found_yaps = false;
    auto head       = u32(sizeof(ExtTableHeader));
    while(head + sizeof(ExtTableTlvHeader) <= len) {
        const auto tlv = (const ExtTableTlvHeader*)(buf.get() + head);
        ensure(tlv->length >= sizeof(ExtTableTlvHeader) && head + tlv->length <= len, "malformed tlv");
        if(tlv->tag == ExtTableTag::YapsTable && tlv->length >= sizeof(ExtTableTlvHeader) + sizeof(YapsTable)) {
            noxx::memcpy(&table.yaps, buf.get() + head + sizeof(ExtTableTlvHeader), sizeof(YapsTable));
            found_yaps = true;
        }
        head += tlv->length;
    }
    ensure(found_yaps, "no yaps table tlv");
    return table;
}
} // namespace halow
