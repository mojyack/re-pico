#include "math.hpp"
#include "ssi.hpp"

extern "C" {
__attribute__((section(".boot2"))) auto boot_stage2() -> void {
    SSI_REGS.ssi_enable   = 0;
    SSI_REGS.baud_rate    = 4;
    SSI_REGS.control0     = ssi::Control0TransferMode::EEPROM_READ << math::log2<ssi::Control0::TMOD> | 31 << math::log2<ssi::Control0::DFS_32>;
    SSI_REGS.spi_control0 = 6 /*24-bits*/ << math::log2<ssi::SPIControl0::ADDR_L> |
                            ssi::SPIControl0InstructionLength::_8B << math::log2<ssi::SPIControl0::INST_L> |
                            3 << math::log2<ssi::SPIControl0::XIP_CMD>;
    SSI_REGS.ssi_enable = 1;

    const auto entry = (void (*)())0x10000101;
    entry();
}

__attribute__((section(".crc"))) u8 crc[4] = {'c', 'r', 'c', 'p'};
}
