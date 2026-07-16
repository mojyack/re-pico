// linux host harness: bridges a tap device into net::Stack and drives it with
// coop, mirroring the on-board embedding (rx loop + timer task on one Runner).
//
//   sudo ip tuntap add mode tap user $USER name tap0
//   sudo ip link set tap0 up
//   sudo ip addr add 192.168.7.1/24 dev tap0
//   ./build/tap-host tap0 192.168.7.2 [192.168.7.1]
//
// then send traffic from the peer, e.g. `ping 192.168.7.2`; the optional third
// argument makes the board ping that address once a second.
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <coop/platform.hpp>
#include <coop/promise.hpp>
#include <coop/runner.hpp>
#include <coop/timer.hpp>
#include <net/icmp.hpp>
#include <net/stack.hpp>

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
struct TapNetIf {
    net::NetIf netif;
    int        fd = -1;

    static auto tx(net::NetIf& netif, net::AutoPacket packet) -> bool {
        constexpr auto error_value = false;

        auto& self = *(TapNetIf*)netif.driver;
        ensure(write(self.fd, packet->data(), packet->len) == packet->len, "tap write failed");
        return true;
    }

    auto init(const char* const name) -> bool {
        constexpr auto error_value = false;

        fd = open("/dev/net/tun", O_RDWR | O_NONBLOCK);
        ensure(fd >= 0, "cannot open /dev/net/tun");

        auto ifr      = ifreq();
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
        ensure(ioctl(fd, TUNSETIFF, &ifr) >= 0, "TUNSETIFF failed (is the tap created?)");

        netif.mac     = {0x02, 0x00, 0x00, 0xe0, 0x0b, 0x05}; // locally administered
        netif.link_up = true;
        netif.driver  = this;
        netif.tx      = tx;
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
// stands in for the board's interrupt-driven driver rx task.
auto reader_task(TapNetIf& tap) -> coop::Async<void> {
    while(true) {
        auto packet = net::AutoPacket(tap.rx_pop());
        if(packet.get() == nullptr) {
            co_await coop::sleep_ms(1);
            continue;
        }
        printf("rx %u bytes\n", unsigned(packet->len));
        hexdump({packet->data(), packet->len});
        tap.netif.rx(tap.netif, noxx::move(packet));
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
        if(!net::icmp::send_echo(stack, target, id, seq, 32)) {
            printf("ping: send failed (packet pool?)\n");
        }
        seq += 1;
        co_await coop::sleep_ms(1000);
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
    stack.init(tap.netif);
    if(argc >= 3) {
        unwrap(addr, net::parse_ip(argv[2]));
        stack.addr    = addr;
        stack.netmask = {255, 255, 255, 0};
    }

    auto runner = coop::Runner();
    ensure(runner.push_task(stack.run()));
    ensure(runner.push_task(stack.timer_task()));
    ensure(runner.push_task(reader_task(tap)));

    // optional outbound ping target: one echo request per second
    if(argc >= 4) {
        unwrap(target, net::parse_ip(argv[3]));
        stack.on_icmp_echo_reply = on_echo_reply;
        ensure(runner.push_task(ping_task(stack, target)));
    }

    printf("tap-host: bridging %s, waiting for frames\n", argv[1]);
    runner.run();
    return 0;
}
