#include <hal/spi.hpp>

#include "../hw/spi.hpp"

namespace spi {
auto xfer(const u8 data) -> u8 {
    constexpr auto timeout = u32(1'000'000);
    for(auto i = u32(0); !(SPI2_REGS.status & hw::spi::Status::TXPacket); i += 1) {
        if(i == timeout) {
            return 0xff;
        }
    }
    *(volatile u8*)&SPI2_REGS.transmit_data = data;
    for(auto i = u32(0); !(SPI2_REGS.status & hw::spi::Status::RXPacket); i += 1) {
        if(i == timeout) {
            return 0xff;
        }
    }
    return *(const volatile u8*)&SPI2_REGS.receive_data;
}
} // namespace spi
