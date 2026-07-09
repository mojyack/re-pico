#include <hal/sleep.hpp>
#include <halow-fw-blob.hpp>
#include <inflate.hpp>
#include <noxx/unique-ptr.hpp>

#include "firmware.hpp"
#include "halow.hpp"

#include <noxx/assert.hpp>

namespace halow {
namespace {
// mm8108 firmware-boot registers (ref morse_driver/mm8108/mm8108.c)
constexpr auto reg_manifest_ptr = u32(0x00002d40);
constexpr auto reg_aon          = u32(0x00002114); // 2 words cleared before boot
constexpr auto reg_aon_count    = u32(2);
constexpr auto reg_aon_latch    = u32(0x00405020);
constexpr auto reg_aon_mask     = u32(0x1);
constexpr auto reg_msi          = u32(0x00004100); // message signalled interrupt
constexpr auto reg_msi_host_int = u32(0x1);

// host_table field offsets (ref morse_driver/hw.h struct host_table)
constexpr auto host_tbl_magic      = u32(0);
constexpr auto host_tbl_fw_version = u32(4);

// latch the always-on register writes into the chip (ref morse_hw_toggle_aon_latch)
auto toggle_aon_latch() -> bool {
    constexpr auto error_value = false;

    unwrap(latch, read_u32(reg_aon_latch));
    ensure(write_u32(reg_aon_latch, latch & ~reg_aon_mask));
    usleep(5000);
    ensure(write_u32(reg_aon_latch, latch | reg_aon_mask));
    usleep(5000);
    ensure(write_u32(reg_aon_latch, latch & ~reg_aon_mask));
    usleep(5000);
    return true;
}

// clear always-on state then kick the firmware CPU (ref morse_firmware_trigger)
auto firmware_trigger() -> bool {
    constexpr auto error_value = false;

    for(auto i = u32(0); i < reg_aon_count; i += 1) {
        ensure(write_u32(reg_aon + i * 4, 0));
    }
    ensure(toggle_aon_latch());
    ensure(write_u32(reg_msi, reg_msi_host_int));
    usleep(5000);
    return true;
}

// inflate and upload segments
auto load_segments(const FwSegment* const segments, const u32 count) -> bool {
    constexpr auto error_value = false;

    for(auto i = u32(0); i < count; i += 1) {
        const auto& seg = segments[i];
        const auto  buf = noxx::make_unique_array<u8>(seg.orig_len);
        ensure(buf);
        ensure(inflate({halow_fw_blob + seg.offset, seg.comp_len}, {buf.get(), seg.orig_len}));
        ensure(write_multi(seg.addr, buf.get(), seg.orig_len));
    }
    return true;
}
} // namespace

auto load_firmware() -> noxx::Optional<FirmwareInfo> {
    constexpr auto error_value = noxx::nullopt;

    // let get_host_table_ptr detect the firmware republishing the pointer
    ensure(write_u32(reg_manifest_ptr, 0));

    // segments ship as independent raw-DEFLATE streams, inflated on the fly
    ensure(load_segments(fw_segments, fw_segments_count));
    ensure(load_segments(bcf_segments, bcf_segments_count));

    ensure(firmware_trigger());

    // poll for the firmware to publish its host table pointer (up to ~1.5s)
    auto host_table_ptr = u32(0);
    for(auto i = u32(0); i < 300; i += 1) {
        unwrap(ptr, read_u32(reg_manifest_ptr));
        if(ptr != 0) {
            host_table_ptr = ptr;
            break;
        }
        usleep(5000);
    }
    ensure(host_table_ptr != 0);

    unwrap(magic, read_u32(host_table_ptr + host_tbl_magic));
    unwrap(version, read_u32(host_table_ptr + host_tbl_fw_version));
    return FirmwareInfo{host_table_ptr, magic, version};
}
} // namespace halow
