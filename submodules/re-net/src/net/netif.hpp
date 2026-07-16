// the driver contract: one ethernet-like interface as seen by the stack
#pragma once
#include <coop/generator.hpp>

#include "mac-addr.hpp"
#include "packet.hpp"

namespace net {
struct Stack;

struct NetIf {
    Stack* stack = nullptr;

    virtual auto is_up() const -> bool                      = 0;
    virtual auto get_mac_addr() const -> MacAddrRef         = 0;
    virtual auto tx(AutoPacket packet) -> coop::Async<bool> = 0; // stack to driver
    auto         rx(AutoPacket packet) -> coop::Async<void>;     // driver to stack
};
} // namespace net
