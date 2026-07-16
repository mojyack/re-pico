#include <connect/eapol.hpp>
#include <connect/sae.hpp>
#include <console.hpp>
#include <coop/io.hpp>
#include <coop/promise.hpp>
#include <coop/task-handle.hpp>
#include <coop/timer.hpp>
#include <hal/rng.hpp>
#include <hal/uart.hpp>
#include <halow/command.hpp>
#include <halow/connect.hpp>
#include <halow/firmware.hpp>
#include <halow/halow.hpp>
#include <halow/host-table.hpp>
#include <halow/netif.hpp>
#include <halow/scan.hpp>
#include <halow/yaps.hpp>
#include <net/arp.hpp>
#include <net/ethernet.hpp>
#include <net/icmp.hpp>
#include <net/ip.hpp>
#include <net/packet.hpp>
#include <net/stack.hpp>
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

auto halow_host_table = noxx::Optional<halow::HostTable>();
auto netstack         = net::Stack();

// icmp echo reply sink for `net ping`; runs in link_task context, so blocking i/o
auto on_ping_reply(net::Stack& /*self*/, const net::IPv4Addr src, const u16 /*id*/, const u16 seq, const noxx::Span<const u8> data) -> void {
    printf_blocking<"ping reply from {}: seq={} {} bytes\n">(src, seq, data.size());
}

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
    halow init                       init pins, download firmwares, start command channel
    halow ver                        query firmware version over command channel
    halow tx [len]                   send a dummy loopback frame over the yaps tx queue
    halow rx                         pop one pending from-chip frame and hexdump it
    halow stat                       dump the yaps status register block
    halow scan [ssid]                scan for s1g access points
    halow connect <ssid> [password]  associate (open, or WPA3-SAE with a password)
    halow disconnect                 deauthenticate and tear the link down
    halow link                       print link status
    halow keepalive                  send a qos-null keepalive to the ap
  net ...           network utilities
    net up                           bring the ip stack up over the halow link
    net ip <addr> [mask [gw]]        configure the ip address
    net ping <addr> [count]          send icmp echo requests
    net arp [ipv4]                   broadcast a who-has, or print the arp table
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
            // init pins
            co_ensure(prepare_pins_for_halow());
            co_ensure(co_await halow::init());
            co_await print("chip initialized\n");

            // download firmware blob and bcf
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

            // start command channel
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
                const auto hdr = halow::parse_skb_header(*packet);
                if(hdr != nullptr) {
                    co_ensure(co_await printf<"frame chan 0x{02x} len {} offset {} rssi {} freq {}00khz\n">(hdr->channel, hdr->len, hdr->offset, i16(hdr->rx_status.rssi), hdr->rx_status.freq_100khz));
                }
                co_await hexdump(packet->data(), packet->len);
            }
        } else if(elms[1] == "scan") {
            co_ensure(halow_host_table, "run halow cmd first");
            const auto ssid    = elms.size() >= 3 ? elms[2] : noxx::StringView();
            auto       results = noxx::Array<halow::ScanResult, 8>();
            co_unwrap(count, co_await halow::scan((*halow_host_table).mac, ssid, results));
            for(auto i = usize(0); i < count; i += 1) {
                const auto& res = results[i];
                co_ensure(co_await printf<"{} rssi {} freq {}00khz bcnint {} '{}'\n">(res.bssid, res.rssi, res.freq_100khz, res.beacon_interval, noxx::StringView((const char*)res.ssid, res.ssid_len)));
            }
            co_ensure(co_await printf<"{} access points found\n">(count));
        } else if(elms[1] == "connect") {
            co_ensure(halow_host_table, "run halow cmd first");
            co_ensure(elms.size() >= 3, "usage: halow connect <ssid> [password]");
            const auto password = elms.size() >= 4 ? elms[3] : noxx::StringView();
            co_ensure(co_await halow::connect((*halow_host_table).mac, elms[2], password));
            const auto& link = halow::link_status();
            co_ensure(co_await printf<"connected to {} aid {} on {}khz\n">(link.bssid, link.aid, link.freq_khz));
        } else if(elms[1] == "disconnect") {
            co_ensure(co_await halow::disconnect());
            co_await print("disconnected\n");
        } else if(elms[1] == "keepalive") {
            co_ensure(co_await halow::send_keepalive());
            co_await print("keepalive sent\n");
        } else if(elms[1] == "link") {
            const auto& link = halow::link_status();
            if(!link.up) {
                co_await print(halow::link_desynced() ? "link down (rx desynced, reboot required)\n" : "link down\n");
            } else {
                co_ensure(co_await printf<"link up: bssid {} aid {} {}khz\n">(link.bssid, link.aid, link.freq_khz));
            }
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
    } else if(elms[0] == "net") {
        co_ensure(elms.size() >= 2, "usage: net <up|ip|ping|arp>");
        if(elms[1] == "up") {
            co_ensure(halow::link_status().up, "not connected");
            // attaching the stack makes link_task deliver rx frames to it
            netstack.init(halow::netif);
            const auto runner = co_await coop::reveal_runner();
            co_ensure(runner->push_task(netstack.timer_task()));
            co_ensure(co_await printf<"net up on {}\n">(halow::netif.get_mac_addr()));
        } else if(elms[1] == "ip") {
            co_ensure(elms.size() >= 3, "usage: net ip <addr> [netmask [gateway]]");
            co_unwrap(addr, net::parse_ip(elms[2]));
            netstack.addr = addr;
            auto netmask  = net::IPv4Addr{255, 255, 255, 0};
            if(elms.size() >= 4) {
                co_unwrap(m, net::parse_ip(elms[3]));
                netmask = m;
            }
            netstack.netmask = netmask;
            if(elms.size() >= 5) {
                co_unwrap(gw, net::parse_ip(elms[4]));
                netstack.gateway = gw;
            }
            const auto& a = netstack.addr;
            const auto& n = netstack.netmask;
            co_ensure(co_await printf<"ip {} netmask {}\n">(a, n));
        } else if(elms[1] == "ping") {
            co_ensure(netstack.netif->is_up(), "run net up first");
            co_ensure(elms.size() >= 3, "usage: net ping <addr> [count]");
            co_unwrap(target, net::parse_ip(elms[2]));
            auto count = u32(4);
            if(elms.size() >= 4) {
                co_unwrap(v, noxx::from_chars<u32>(elms[3]));
                count = v;
            }
            netstack.on_icmp_echo_reply = on_ping_reply;
            for(auto i = u32(0); i < count; i += 1) {
                if(!co_await net::icmp::send_echo(netstack, target, 0xbeef, u16(i), 32)) {
                    co_await print("ping: send failed\n");
                }
                co_await coop::sleep_ms(1000);
            }
        } else if(elms[1] == "arp") {
            if(elms.size() >= 3) {
                // broadcast a who-has; the reply lands in the table
                co_ensure(netstack.netif != nullptr, "run net up first");
                co_unwrap(ip, net::parse_ip(elms[2]));
                co_ensure(co_await net::arp::request(netstack, ip));
                co_await print("arp request sent\n");
            } else {
                for(auto& e : netstack.arp.entries.data) {
                    if(e.state == net::arp::State::Free) {
                        continue;
                    }
                    co_ensure(co_await printf<"{} -> {} {}\n">(e.ip, e.mac, e.state == net::arp::State::Resolved ? "resolved" : "pending"));
                }
            }
        } else {
            co_ensure(false, "usage: net <up|ip|ping|arp>");
        }
    } else if(elms[0] == "mac") {
        co_ensure(halow_host_table, "run halow cmd first");
        const auto& mac = (*halow_host_table).mac;
        co_ensure(co_await printf<"{}\n">(mac));
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
    rng::init();
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
