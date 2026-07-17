// linux host harness: bridges a tap device into net::Stack and drives it with
// coop, mirroring the on-board embedding (rx loop + timer task on one Runner).
//
//   sudo ip tuntap add mode tap user $USER name tap0
//   sudo ip link set tap0 up
//   sudo ip addr add 192.168.7.1/24 dev tap0
//   ./build/tap-host tap0 192.168.7.2 [192.168.7.1]
//
// then send traffic from the peer, e.g. `ping 192.168.7.2`; the optional third
// argument makes the board ping that address once a second. passing "dhcp" as
// the address instead runs the dhcp client against a server on the tap, e.g.
//   dnsmasq -d -i tap0 --bind-interfaces --dhcp-range=192.168.7.100,192.168.7.150,1h
// instead of a ping target, "tcp:PORT" runs a tcp echo server (`nc 192.168.7.2
// PORT`) and "tcp:IP:PORT" connects out, sends a greeting and prints the reply
// (`nc -l 192.168.7.1 PORT`)
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <coop/platform.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/timer.hpp>
#include <net/dhcp.hpp>
#include <net/icmp.hpp>
#include <net/stack.hpp>
#include <net/tcp.hpp>

#include <noxx/assert.hpp>
#include <noxx/malloc.hpp>

namespace {
auto now_ms() -> u64 {
    return coop::now_us() / 1000;
}

auto hexdump(const noxx::Span<const u8> data) -> void {
    for(auto i = usize(0); i < data.size(); i += 16) {
        printf("  %04zx:", i);
        for(auto c = i; c < data.size() && c < i + 16; c += 1) {
            printf(" %02x", data[c]);
        }
        printf("\n");
    }
}

// NetIf backed by a non-blocking tap file descriptor
struct TapNetIf : net::NetIf {
    int fd = -1;

    auto is_up() const -> bool override {
        return true;
    }
    auto get_mac_addr() const -> net::MacAddrRef override {
        static constexpr auto mac = net::MacAddr{0x02, 0x00, 0x00, 0xe0, 0x0b, 0x05}; // locally administered
        return mac;
    }
    auto tx(net::AutoPacket packet) -> coop::Async<bool> override {
        constexpr auto error_value = false;

        co_ensure(write(fd, packet->data(), packet->len) == packet->len, "tap write failed");
        co_return true;
    }

    auto init(const char* const name) -> bool {
        constexpr auto error_value = false;

        fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
        ensure(fd >= 0, "cannot open /dev/net/tun");

        auto ifr      = ifreq();
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
        ensure(ioctl(fd, TUNSETIFF, &ifr) >= 0, "TUNSETIFF failed (is the tap created?)");
        return true;
    }

    // read one frame from the tap into a fresh packet; null if none pending
    // (EAGAIN on the non-blocking fd is the normal idle case, not an error)
    auto rx_pop() -> net::Packet* {
        constexpr auto error_value = nullptr;

        auto packet = net::AutoPacket(net::packet_alloc());
        ensure(packet.get() != nullptr);
        const auto n = read(fd, packet->buf, net::packet_capacity);
        if(n <= 0) {
            return nullptr;
        }
        packet->head = 0;
        packet->len  = u16(n);
        return packet.release();
    }
};

// pump the tap fd into the stack: non-blocking reads, yielding when idle. this
// is the rx loop — it co_awaits netif.rx, which demuxes the frame inline. stands
// in for the board's interrupt-driven driver rx task.
auto reader_task(TapNetIf& tap) -> coop::Async<void> {
    while(true) {
        auto packet = net::AutoPacket(tap.rx_pop());
        if(packet.get() == nullptr) {
            co_await coop::sleep_ms(1);
            continue;
        }
        printf("rx %u bytes\n", unsigned(packet->len));
        hexdump({packet->data(), packet->len});
        co_await tap.rx(noxx::move(packet));
    }
}

// outbound ping state: send times keyed by low sequence byte
auto ping_sent_at = noxx::Array<u64, 256>();

auto on_echo_reply(net::Stack& /*self*/, const net::IPv4Addr src, const u16 id, const u16 seq, const noxx::Span<const u8> data) -> void {
    const auto rtt = now_ms() - ping_sent_at[seq & 0xff];
    printf("ping reply from %u.%u.%u.%u: id=%u seq=%u %zu bytes rtt=%llu ms\n",
           src[0], src[1], src[2], src[3], id, seq, data.size(), (unsigned long long)rtt);
}

auto ping_task(net::Stack& stack, const net::IPv4Addr target) -> coop::Async<void> {
    const auto id  = u16(0xbeef);
    auto       seq = u16(0);
    while(true) {
        ping_sent_at[seq & 0xff] = now_ms();
        if(!co_await net::icmp::send_echo(stack, target, id, seq, 32)) {
            printf("ping: send failed (packet pool?)\n");
        }
        seq += 1;
        co_await coop::sleep_ms(1000);
    }
}

// serve one tcp connection at a time, echoing everything back
auto tcp_echo_task(net::Stack& stack, const u16 port) -> coop::Async<void> {
    auto conn = net::tcp::Conn();
    while(true) {
        // a prior connection may linger in time-wait until tick() reaps it
        while(conn.state != net::tcp::State::Closed) {
            co_await coop::sleep_ms(500);
        }
        if(!conn.listen(stack, port)) {
            co_return;
        }
        printf("tcp: listening on %u\n", port);
        if(!co_await conn.accept(stack)) {
            continue;
        }
        const auto& a = conn.remote_addr;
        printf("tcp: accepted from %u.%u.%u.%u:%u\n", a[0], a[1], a[2], a[3], conn.remote_port);
        auto buf = noxx::Array<u8, 1024>();
        while(true) {
            const auto count = co_await conn.recv(stack, buf);
            if(!count || *count == 0) {
                break;
            }
            printf("tcp: echoing %zu bytes\n", *count);
            if(!co_await conn.send(stack, {buf.data, *count})) {
                break;
            }
        }
        co_await conn.close(stack);
        printf("tcp: connection closed\n");
    }
}

// connect out, send a greeting and print whatever comes back until eof
auto tcp_client_task(net::Stack& stack, const net::IPv4Addr addr, const u16 port) -> coop::Async<void> {
    auto conn = net::tcp::Conn();
    if(!co_await conn.connect(stack, addr, port)) {
        printf("tcp: connect failed\n");
        co_return;
    }
    printf("tcp: connected from port %u\n", conn.local_port);
    const auto greeting = noxx::StringView("hello from re-net\n");
    if(!co_await conn.send(stack, {(const u8*)greeting.data(), greeting.size()})) {
        printf("tcp: send failed\n");
    }
    auto buf = noxx::Array<u8, 1024>();
    while(true) {
        const auto count = co_await conn.recv(stack, buf);
        if(!count || *count == 0) {
            break;
        }
        fwrite(buf.data, 1, *count, stdout);
        fflush(stdout);
    }
    co_await conn.close(stack);
    printf("tcp: done\n");
}

// announce dhcp state changes; the client task itself is silent
auto dhcp_report_task(net::dhcp::Client& client, net::Stack& stack) -> coop::Async<void> {
    auto last = net::dhcp::State::Idle;
    while(true) {
        if(client.state != last) {
            last = client.state;
            if(last == net::dhcp::State::Bound) {
                const auto& a = stack.addr;
                const auto& m = stack.netmask;
                const auto& g = stack.gateway;
                printf("dhcp bound: addr=%u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u lease=%us\n",
                       a[0], a[1], a[2], a[3], m[0], m[1], m[2], m[3], g[0], g[1], g[2], g[3],
                       unsigned(client.lease.lease_s));
            } else {
                printf("dhcp state=%u\n", unsigned(last));
            }
        }
        co_await coop::sleep_ms(100);
    }
}
} // namespace

auto main(const int argc, const char* const argv[]) -> int {
    constexpr auto error_value = 1;

    ensure(argc >= 2, "usage: tap-host TAPDEV [IPADDR [PINGTARGET]]");

    static auto heap = noxx::Array<u8, 1 << 20>(); // coop task/frame allocations
    noxx::set_heap(heap.data, heap.size());
    net::packet_pool_init();

    auto tap = TapNetIf();
    ensure(tap.init(argv[1]));

    auto stack = net::Stack();
    stack.init(tap);

    auto runner = coop::Runner();
    ensure(runner.push_task(stack.timer_task()));
    ensure(runner.push_task(reader_task(tap)));

    static auto dhcp_client = net::dhcp::Client();
    if(argc >= 3) {
        if(strcmp(argv[2], "dhcp") == 0) {
            ensure(runner.push_task(dhcp_client.task(stack)));
            ensure(runner.push_task(dhcp_report_task(dhcp_client, stack)));
        } else {
            unwrap(addr, net::parse_ip(argv[2]));
            stack.addr    = addr;
            stack.netmask = {255, 255, 255, 0};
        }
    }

    // optional traffic generator: "tcp:PORT" echo server, "tcp:IP:PORT"
    // client, or a ping target (one echo request per second)
    if(argc >= 4) {
        if(strncmp(argv[3], "tcp:", 4) == 0) {
            const auto rest  = argv[3] + 4;
            const auto colon = strchr(rest, ':');
            if(colon == nullptr) {
                ensure(runner.push_task(tcp_echo_task(stack, u16(atoi(rest)))));
            } else {
                unwrap(addr, net::parse_ip({rest, usize(colon - rest)}));
                ensure(runner.push_task(tcp_client_task(stack, addr, u16(atoi(colon + 1)))));
            }
        } else {
            unwrap(target, net::parse_ip(argv[3]));
            stack.on_icmp_echo_reply = on_echo_reply;
            ensure(runner.push_task(ping_task(stack, target)));
        }
    }

    printf("tap-host: bridging %s, waiting for frames\n", argv[1]);
    runner.run();
    return 0;
}
