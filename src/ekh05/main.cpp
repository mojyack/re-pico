#include <coop/io.hpp>
#include <coop/promise.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <hal/uart.hpp>
#include <halow/command.hpp>
#include <halow/connect.hpp>
#include <halow/firmware.hpp>
#include <halow/halow.hpp>
#include <halow/host-table.hpp>
#include <halow/scan.hpp>
#include <halow/yaps.hpp>
#include <net/arp.hpp>
#include <net/packet.hpp>
#include <noxx/bits.hpp>
#include <noxx/charconv.hpp>
#include <noxx/format.hpp>
#include <noxx/malloc.hpp>
#include <print.hpp>
#include <split.hpp>
#include <uart.hpp>

#include "hal/time.hpp"
#include "hal/uart.hpp"
#include "halow.hpp"
#include "hw/dbgmcu.hpp"
#include "hw/gpio.hpp"
#include "hw/nvic.hpp"
#include "hw/scb.hpp"
#include "system.hpp"

#include <noxx/assert.hpp>

// from link.ld
extern "C" void* vector[];
extern u32       heap_start;
extern u32       stack_top;
extern u32       bss_start;
extern u32       bss_end;
extern u32       data_start;
extern u32       data_end;
extern u32       data_load;
extern void (*init_array_start[])();
extern void (*init_array_end[])();

namespace {
template <noxx::comptime::String str, class... Args>
auto printf_blocking(const Args&... args) -> bool {
    constexpr auto error_value = false;
    unwrap(raw, noxx::format<str>(noxx::move(args)...));
    print_blocking(raw.data());
    return true;
}

template <noxx::comptime::String str, class... Args>
auto printf(const Args&... args) -> coop::Async<bool> {
    constexpr auto error_value = false;
    co_unwrap(raw, noxx::format<str>(noxx::move(args)...));
    co_await print(raw.data());
    co_return true;
}

auto get_u8() -> u8 {
    auto buf = noxx::Array<u8, 1>();
    uart::read_blocking(buf);
    return buf[0];
}

auto dump_task_tree(const coop::Task& task, const int indent = 0) -> void {
    for(auto i = 0; i < indent; i += 1) {
        print_blocking(" ");
    }
    printf_blocking<"- task={} parent={} suspend={} obj={} zombie={}\n">((void*)&task, (void*)task.parent, task.suspend_reason.get_index(), task.objective_of, task.zombie);
    for(auto i = usize(0); i < task.children.size(); i += 1) {
        dump_task_tree(*task.children[i], indent + 2);
    }
}

auto read_line() -> coop::Async<noxx::Optional<noxx::String>> {
    constexpr auto error_value = noxx::nullopt;

    auto line = noxx::String();
    while(true) {
        co_ensure(co_await coop::wait_for_io(uart::read_event));
        auto c = u8();
        co_ensure(uart::read({&c, 1}) == 1);
        co_await uart::write_all({&c, 1});
        if(c == '\r' || c == '\n') {
            break;
        } else {
            line.append(c);
        }
    }
    co_return line;
}

auto halow_host_table = noxx::Optional<halow::HostTable>();

auto hexdump(const u8* const data, const usize size) -> coop::Async<bool> {
    constexpr auto error_value = false;

    for(auto i = usize(0); i < size; i += 16) {
        co_ensure(co_await printf<"{04x}:">(u32(i)));
        for(auto c = i; c < i + 16 && c < size; c += 1) {
            co_ensure(co_await printf<" {02x}">(data[c]));
        }
        co_await print("\n");
    }
    co_return true;
}

constexpr auto help = R"(commands:
  help              print this message
  reboot            reboot the board
  halow ...         control mm8108
    halow init      initialize pins
    halow fw        download firmware blob and bcf
    halow cmd       parse host table, start command channel
    halow ver       query firmware version over command channel
    halow tx [len]  send a dummy loopback frame over the yaps tx queue
    halow rx        pop one pending from-chip frame and hexdump it
    halow stat      dump the yaps status register block
    halow scan [ssid]  scan for s1g access points
    halow connect <ssid>  associate with an open s1g access point
    halow disconnect   deauthenticate and tear the link down
    halow dump [sec]   dump received ethernet frames
    halow arp <ipv4>   broadcast an arp request over the link
  mac               print halow mac address
  version           print dbgmcu versions
  ps                print process tree
  mem               dump heap chunks and usage
)";

auto handle_command(noxx::StringView line) -> coop::Async<bool> {
    constexpr auto error_value = false;

    co_unwrap(elms, split(line, " "));
    if(elms.size() == 0) {
        co_return true;
    }

    if(elms[0] == "help") {
        co_await print(help);
    } else if(elms[0] == "reboot") {
        SCB_REGS.app_int_control = BF(hw::scb::AppIntControl::VectKey, hw::scb::AppIntControlVectKey::Key) |
                                   BF(hw::scb::AppIntControl::SysResetReq, 1);
    } else if(elms[0] == "halow") {
        co_ensure(elms.size() >= 2);
        if(elms[1] == "init") {
            co_ensure(prepare_pins_for_halow());
            co_ensure(co_await halow::init());
            co_await print("halow initialized\n");
        } else if(elms[1] == "fw") {
            co_unwrap(id, halow::read_u32(halow::Reg::ChipID));
            co_ensure(co_await printf<"halow chip id 0x{08x}\n">(id));
            co_unwrap(fw, co_await halow::load_firmware());
            co_ensure(co_await printf<"halow host table 0x{08x} magic 0x{08x} fw {}.{}.{}\n">(
                fw.host_table_ptr, fw.magic,
                halow::version_major(fw.version),
                halow::version_minor(fw.version),
                halow::version_patch(fw.version)));
            if(fw.magic != halow::host_magic) {
                co_await print("halow magic mismatch, firmware did not boot\n");
            } else {
                co_await print("halow firmware booted\n");
            }
        } else if(elms[1] == "cmd") {
            co_unwrap(ptr, halow::host_table_ptr());
            co_unwrap(table, halow::parse_host_table(ptr));
            halow::init_yaps(table.yaps);
            halow_host_table.emplace(table);
            co_ensure(co_await printf<"halow fw flags 0x{08x} yaps ysl 0x{08x} yds 0x{08x} status 0x{08x}\n">(
                table.firmware_flags, table.yaps.ysl_addr, table.yaps.yds_addr, table.yaps.status_regs_addr));
            co_ensure(co_await printf<"halow cmd queue {} pool {} pages, reserved page size {}\n">(
                table.yaps.tc_cmd_q_size, table.yaps.tc_cmd_pool_size, table.yaps.reserved_page_size));
            co_await print("halow command channel ready\n");
        } else if(elms[1] == "ver") {
            auto resp = noxx::Array<u8, 132>();
            co_unwrap(len, co_await halow::send_command(halow::CommandId::GetVersion, {nullptr, 0}, {resp.data, resp.size()}));
            co_ensure(len >= 4, "short version response");
            auto str_len = usize(u32(resp[0]) | u32(resp[1]) << 8 | u32(resp[2]) << 16 | u32(resp[3]) << 24);
            str_len      = str_len < len - 4 ? str_len : len - 4;
            while(str_len > 0 && resp[4 + str_len - 1] == '\0') { // fw counts the terminator
                str_len -= 1;
            }
            co_ensure(co_await printf<"halow firmware version '{}'\n">(noxx::StringView((const char*)resp.data + 4, str_len)));
        } else if(elms[1] == "tx") {
            auto len = u32(64);
            if(elms.size() >= 3) {
                co_unwrap(v, noxx::from_chars<u32>(elms[2]));
                len = v;
            }
            const auto packet = net::AutoPacket(net::packet_alloc(halow::tx_headroom));
            co_ensure(packet);
            co_unwrap(body, packet->append(len), "payload too long");
            for(auto i = u32(0); i < len; i += 1) {
                (&body)[i] = i;
            }
            auto status = halow::YapsStatus();
            co_ensure(co_await halow::read_status(status));
            co_ensure(co_await printf<"tx queue before: {} pkts, {} pool pages\n">(status.regs[halow::YapsStatus::TcTxPkts], status.regs[halow::YapsStatus::TcTxPoolPages]));
            const auto sent = co_await halow::yaps_tx(halow::SkbChan::Loopback, *packet);
            co_ensure(sent);
            co_await coop::sleep_ms(10);
            co_ensure(co_await halow::read_status(status));
            co_ensure(co_await printf<"tx queue after:  {} pkts, {} pool pages\n">(
                status.regs[halow::YapsStatus::TcTxPkts], status.regs[halow::YapsStatus::TcTxPoolPages]));
            co_unwrap(ptr, halow::host_table_ptr());
            co_unwrap(magic, halow::read_u32(ptr));
            co_ensure(co_await printf<"fw health: magic 0x{08x} ({})\n">(magic, magic == halow::host_magic ? "ok" : "WEDGED"));
        } else if(elms[1] == "rx") {
            co_unwrap(packet, co_await halow::fetch_rx());
            if(!packet) {
                co_await print("no frame pending\n");
            } else {
                auto hdr_o = halow::parse_skb_header(*packet);
                if(hdr_o) {
                    co_ensure(co_await printf<"frame chan 0x{02x} len {} offset {} rssi {} freq {}00khz\n">((*hdr_o).channel, (*hdr_o).len, (*hdr_o).offset, i16((*hdr_o).rssi), (*hdr_o).freq_100khz));
                }
                co_await hexdump(packet->data(), packet->len);
            }
        } else if(elms[1] == "scan") {
            co_ensure(halow_host_table, "run halow cmd first");
            const auto ssid    = elms.size() >= 3 ? elms[2] : noxx::StringView("", 0);
            auto       results = noxx::Array<halow::ScanResult, 8>();
            co_unwrap(count, co_await halow::scan((*halow_host_table).mac, ssid, results));
            for(auto i = usize(0); i < count; i += 1) {
                const auto& res = results[i];
                co_ensure(co_await printf<"{02x}:{02x}:{02x}:{02x}:{02x}:{02x} rssi {} freq {}00khz bcnint {} '{}'\n">(
                    res.bssid[0], res.bssid[1], res.bssid[2], res.bssid[3], res.bssid[4], res.bssid[5],
                    res.rssi, res.freq_100khz, res.beacon_interval,
                    noxx::StringView((const char*)res.ssid, res.ssid_len)));
            }
            co_ensure(co_await printf<"{} access points found\n">(count));
        } else if(elms[1] == "connect") {
            co_ensure(halow_host_table, "run halow cmd first");
            co_ensure(elms.size() >= 3, "usage: halow connect <ssid>");
            co_ensure(co_await halow::connect((*halow_host_table).mac, elms[2]));
            const auto& link = halow::link_status();
            co_ensure(co_await printf<"connected to {02x}:{02x}:{02x}:{02x}:{02x}:{02x} aid {} on {}khz\n">(
                link.bssid[0], link.bssid[1], link.bssid[2], link.bssid[3], link.bssid[4], link.bssid[5],
                link.aid, link.freq_khz));
        } else if(elms[1] == "disconnect") {
            co_ensure(co_await halow::disconnect());
            co_await print("disconnected\n");
        } else if(elms[1] == "dump") {
            auto seconds = u32(10);
            if(elms.size() >= 3) {
                co_unwrap(v, noxx::from_chars<u32>(elms[2]));
                seconds = v;
            }
            co_ensure(co_await printf<"dumping rx as ethernet frames for {}s\n">(seconds));
            const auto deadline = time::now() + u64(seconds) * 1'000'000;
            auto       popped   = u32(0);
            auto       shown    = u32(0);
            while(time::now() < deadline) {
                co_unwrap(packet, co_await halow::fetch_rx());
                if(!packet) {
                    co_await coop::sleep_ms(10);
                    continue;
                }
                popped += 1;
                if(!halow::eth_from_rx(*packet)) {
                    continue;
                }
                shown += 1;
                const auto p = packet->data();
                co_ensure(co_await printf<"eth {02x}:{02x}:{02x}:{02x}:{02x}:{02x} <- {02x}:{02x}:{02x}:{02x}:{02x}:{02x} type 0x{04x} len {}\n">(
                    p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], p[8], p[9], p[10], p[11],
                    u16(p[12]) << 8 | p[13], packet->len));
                co_await hexdump(p, packet->len);
            }
            co_ensure(co_await printf<"dump done: {} frames popped, {} ethernet\n">(popped, shown));
        } else if(elms[1] == "arp") {
            co_ensure(elms.size() >= 3, "usage: halow arp <target-ipv4> [sender-ipv4]");
            auto parse_ip = [](noxx::StringView str, u8* out) -> bool {
                constexpr auto error_value = false;
                unwrap(parts, split(str, "."));
                ensure(parts.size() == 4, "bad ipv4 address");
                for(auto i = usize(0); i < 4; i += 1) {
                    unwrap(v, noxx::from_chars<u8>(parts[i]));
                    out[i] = v;
                }
                return true;
            };
            auto ip     = noxx::Array<u8, 4>();
            auto sender = noxx::Array<u8, 4>();
            for(auto i = usize(0); i < 4; i += 1) {
                sender[i] = 0;
            }
            co_ensure(parse_ip(elms[2], ip.data));
            if(elms.size() >= 4) {
                co_ensure(parse_ip(elms[3], sender.data));
            }
            const auto& link = halow::link_status();
            co_ensure(link.up, "not connected");
            auto arp = noxx::Array<u8, net::arp::packet_size>();
            co_ensure(net::arp::build_request({arp.data, arp.size()}, link.mac, sender.data, ip.data));
            constexpr auto broadcast = noxx::to_array<u8>({0xff, 0xff, 0xff, 0xff, 0xff, 0xff});
            // "uni" sends the request unicast to the ap itself, which is acked
            // over the air and exercises the reliable data path
            const auto dst = (elms.size() >= 5 && elms[4] == "uni") ? halow::link_status().bssid : broadcast.data;
            co_ensure(co_await halow::eth_tx(dst, net::arp::ethertype, {arp.data, arp.size()}));
            co_await print("arp request sent, use halow dump to see the reply\n");
        } else if(elms[1] == "stat") {
            auto status = halow::YapsStatus();
            co_ensure(co_await halow::read_status(status));
            constexpr auto names = noxx::to_array({
                "tc tx pool pages",
                "tc cmd pool pages",
                "tc beacon pool pages",
                "tc mgmt pool pages",
                "fc rx pool pages",
                "fc resp pool pages",
                "fc tx-sts pool pages",
                "fc aux pool pages",
                "tc tx pkts",
                "tc cmd pkts",
                "tc beacon pkts",
                "tc mgmt pkts",
                "fc pkts",
                "fc done pkts",
                "fc rx bytes",
                "tc crc fail",
                "ysl status",
                "lock",
            });
            for(auto i = u32(0); i < halow::YapsStatus::Count; i += 1) {
                co_ensure(co_await printf<"  {}: {}\n">(names[i], status.regs[i]));
            }
            co_ensure(co_await printf<"packet pool: {} free\n">(net::packet_pool_avail()));
        } else {
            co_ensure(false, "invalid halow command");
        }
    } else if(elms[0] == "mac") {
        co_ensure(halow_host_table, "run halow cmd first");
        const auto& mac = (*halow_host_table).mac;
        co_ensure(co_await printf<"{02x}:{02x}:{02x}:{02x}:{02x}:{02x}\n">(mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]));
    } else if(elms[0] == "version") {
        const auto id  = FB(hw::dbgmcu::IDCode::DeviceID, DBGMCU_REGS.idcode);
        const auto rev = FB(hw::dbgmcu::IDCode::Revision, DBGMCU_REGS.idcode);
        co_ensure(co_await printf<"idcode 0x{04x} revision 0x{04x}\n">(id, rev));
    } else if(elms[0] == "ps") {
        dump_task_tree((co_await coop::reveal_runner())->root);
    } else if(elms[0] == "mem") {
        noxx::heap_walk(nullptr, [](void*, const void* addr, const usize size, const bool is_free) {
            printf_blocking<"  {} {} bytes {}\n">(addr, size, is_free ? "free" : "used");
        });
        const auto stats = noxx::heap_stats();
        co_ensure(co_await printf<"used {} bytes ({} chunks), free {} bytes ({} chunks), largest free {} bytes\n">(
            stats.used, stats.used_chunks, stats.free, stats.free_chunks, stats.largest_free));
    } else {
        co_ensure(co_await printf<"invalid command '{}': try help\n">(elms[0]));
    }

    co_return true;
}

auto console_task_main() -> coop::Async<bool> {
    constexpr auto error_value = false;
loop:
    co_await print("% ");
    co_unwrap(line, co_await read_line());
    co_await print("\n");
    if(line.size() == 0) {
        goto loop;
    }
    co_await handle_command(line);
    goto loop;
    co_return true;
}

auto entry() -> void {
    for(auto i = u32(0); i < &bss_end - &bss_start; i += 1) {
        (&bss_start)[i] = 0;
    }
    for(auto i = u32(0); i < &data_end - &data_start; i += 1) {
        (&data_start)[i] = (&data_load)[i];
    }
    for(auto init = init_array_start; init != init_array_end; init += 1) {
        (*init)();
    }
    SCB_REGS.vector_table_offset = u32(usize(&vector[0]));       // flash or SRAM, per link script
    const auto heap_end          = (usize)&stack_top - 8 * 1024; // 8KB for stack
    noxx::set_heap(&heap_start, heap_end - (usize)&heap_start);
    enable_leds();
    GPIOE_REGS.bit_set_reset = 1 << led_blue;
    init_system();
    uart::init(921600);
    time::start_systick();
    net::packet_pool_init();

    print_blocking("ready\n");
loop:
    auto runner = coop::Runner();
    runner.push_task(console_task_main());
    runner.run();
    goto loop;
}
} // namespace

extern "C" {
[[noreturn]] auto default_int_handler() -> void {
    enable_leds();
    while(true) {
        for(auto i = 0; i < 500000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << led_red;
        }
        for(auto i = 0; i < 500000; i += 1) {
            GPIOE_REGS.bit_set_reset = 1 << (led_red + 16);
        }
    }
}

// internal interruptions
__attribute__((weak, alias("default_int_handler"))) auto nmi_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto hard_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto mem_manage_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto bus_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto usage_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto secure_fault_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto debug_monitor_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto pend_sv_call_handler() -> void;
__attribute__((weak, alias("default_int_handler"))) auto systick_handler() -> void;

__attribute__((section(".vector"))) void* vector[16 + u32(hw::nvic::IRQ::LpUart1) + 1] = {
    (void*)&stack_top,
    (void*)&entry,
    (void*)&nmi_handler,
    (void*)&hard_fault_handler,
    (void*)&mem_manage_handler,
    (void*)&bus_fault_handler,
    (void*)&usage_fault_handler,
    (void*)&secure_fault_handler,
    nullptr,
    nullptr,
    nullptr,
    (void*)&sv_call_handler,
    (void*)&debug_monitor_handler,
    nullptr,
    (void*)&pend_sv_call_handler,
    (void*)&systick_handler,
    [16 + u32(hw::nvic::IRQ::LpUart1)] = (void*)&uart::lpuart1_handler,
};
}
