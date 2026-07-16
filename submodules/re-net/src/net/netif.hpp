// the driver contract: one ethernet-like interface as seen by the stack
#pragma once
#include "mac-addr.hpp"
#include "packet.hpp"

namespace net {
// the driver fills mac/link_up/tx and calls rx() (installed by Stack::init)
// for every received ethernet frame. single-threaded: the driver and the
// stack run on the same scheduler, so no locking anywhere.
struct NetIf {
    MacAddr mac     = {};
    bool    link_up = false;

    void* driver = nullptr; // driver private context
    void* stack  = nullptr; // owning net::Stack, set by Stack::init

    // driver-provided: transmit one ethernet frame, consuming the packet
    auto (*tx)(NetIf& self, AutoPacket packet) -> bool = nullptr;
    // stack-provided rx delivery hook, consuming the packet
    auto (*rx)(NetIf& self, AutoPacket packet) -> void = nullptr;
};
} // namespace net
