// linux host harness (phase n0): bridges a tap device into net::Stack and
// hexdumps every received frame.
//
//   sudo ip tuntap add mode tap user $USER name tap0
//   sudo ip link set tap0 up
//   sudo ip addr add 192.168.7.1/24 dev tap0
//   ./build/tap-host tap0 192.168.7.2
//
// then send traffic from the peer, e.g. `ping 192.168.7.2`
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include <net/stack.hpp>

#include <noxx/assert.hpp>

namespace {
auto now_ms() -> u64 {
    auto ts = timespec();
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return u64(ts.tv_sec) * 1000 + u64(ts.tv_nsec) / 1000000;
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

// NetIf backed by a tap file descriptor
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

        fd = open("/dev/net/tun", O_RDWR);
        ensure(fd >= 0, "cannot open /dev/net/tun");

        auto ifr = ifreq();
        ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
        strncpy(ifr.ifr_name, name, IFNAMSIZ - 1);
        ensure(ioctl(fd, TUNSETIFF, &ifr) >= 0, "TUNSETIFF failed (is the tap created?)");

        netif.mac     = {0x02, 0x00, 0x00, 0xe0, 0x0b, 0x05}; // locally administered
        netif.link_up = true;
        netif.driver  = this;
        netif.tx      = tx;
        return true;
    }

    // read one frame from the tap into a fresh packet; null on failure
    auto rx_pop() -> net::Packet* {
        constexpr auto error_value = nullptr;

        auto packet = net::AutoPacket(net::packet_alloc());
        ensure(packet.get() != nullptr);
        const auto n = read(fd, packet->buf, net::packet_capacity);
        ensure(n > 0, "tap read failed");
        packet->head = 0;
        packet->len  = u16(n);
        return packet.release();
    }
};
} // namespace

auto main(const int argc, const char* const argv[]) -> int {
    constexpr auto error_value = 1;

    ensure(argc >= 2, "usage: tap-host TAPDEV [IPADDR]");

    net::packet_pool_init();

    auto tap = TapNetIf();
    ensure(tap.init(argv[1]));

    auto stack = net::Stack();
    stack.init(tap.netif);
    if(argc >= 3) {
        unwrap(addr, net::parse_ip(argv[2]));
        stack.addr = addr;
    }

    printf("tap-host: bridging %s, waiting for frames\n", argv[1]);
    while(true) {
        auto fds = fd_set();
        FD_ZERO(&fds);
        FD_SET(tap.fd, &fds);
        auto timeout = timeval{.tv_sec = 0, .tv_usec = 100 * 1000};
        const auto r = select(tap.fd + 1, &fds, nullptr, nullptr, &timeout);
        ensure(r >= 0, "select failed");
        if(r > 0) {
            auto packet = net::AutoPacket(tap.rx_pop());
            if(packet.get() != nullptr) {
                printf("rx %u bytes\n", unsigned(packet->len));
                hexdump({packet->data(), packet->len});
                tap.netif.rx(tap.netif, noxx::move(packet));
            }
        }
        while(stack.dispatch()) {
        }
        stack.tick(now_ms());
    }
}
