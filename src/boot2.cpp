#include "m0plus.hpp"
#include "math.hpp"
#include "spi-flash/w25q80.hpp"
#include "ssi.hpp"

#define BF(field, value) value << math::log2<field>

extern "C" {
namespace {
auto wait_and_read(u8 count) -> u32;
auto read_flash_sreg(u8 status_command) -> u8;
} // namespace

__attribute__((section(".boot2"))) auto boot_stage2() -> void {
    SSI_REGS.ssi_enable = 0;
    SSI_REGS.baud_rate  = 4;
    SSI_REGS.control0   = BF(ssi::Control0::TMOD, ssi::Control0TransferMode::TX_AND_RX) | BF(ssi::Control0::DFS_32, 7);
    SSI_REGS.ssi_enable = 1;

    const auto st1 = read_flash_sreg(flash::Instructions::ReadStatusRegister1);
    const auto st2 = read_flash_sreg(flash::Instructions::ReadStatusRegister2);
    if(!(st2 & flash::StatusRegister2::QE)) {
        SSI_REGS.data_register[0] = flash::Instructions::WriteEnable;
        wait_and_read(1);
        SSI_REGS.data_register[0] = flash::Instructions::WriteStatusRegister;
        SSI_REGS.data_register[0] = st1;
        SSI_REGS.data_register[0] = st2 | flash::StatusRegister2::QE;
        SSI_REGS.data_register[0] = flash::Instructions::WriteDisable;
        while((read_flash_sreg(flash::Instructions::ReadStatusRegister1) & flash::StatusRegister1::BUSY) != 0) {
        }
    }

    // dummy read
    SSI_REGS.ssi_enable = 0;
    SSI_REGS.control0 =
        BF(ssi::Control0::TMOD, ssi::Control0TransferMode::EEPROM_READ) |
        BF(ssi::Control0::DFS_32, 31) |
        BF(ssi::Control0::SPI_FRF, ssi::Control0SPIFrameFormat::QUAD);
    SSI_REGS.spi_control0 =
        BF(ssi::SPIControl0::ADDR_L, 8 /*32-bits*/) |
        BF(ssi::SPIControl0::INST_L, ssi::SPIControl0InstructionLength::_8B) |
        BF(ssi::SPIControl0::TRANS_TYPE, ssi::SPIControl0TransType::_1C2A) |
        BF(ssi::SPIControl0::WAIT_CYCLES, 4);
    SSI_REGS.ssi_enable = 1;

    SSI_REGS.data_register[0] = flash::Instructions::FastReadQuadIO;
    SSI_REGS.data_register[0] = flash::ReadMode::Continuous;
    wait_and_read(2);

    // configure
    SSI_REGS.ssi_enable = 0;
    SSI_REGS.spi_control0 =
        BF(ssi::SPIControl0::XIP_CMD, flash::ReadMode::Continuous) |
        BF(ssi::SPIControl0::ADDR_L, 8 /*32-bits*/) |
        BF(ssi::SPIControl0::WAIT_CYCLES, 4) |
        BF(ssi::SPIControl0::INST_L, ssi::SPIControl0InstructionLength::_NONE) |
        BF(ssi::SPIControl0::TRANS_TYPE, ssi::SPIControl0TransType::_2C2A);
    SSI_REGS.ssi_enable = 1;

    M0PLUS_REGS.vector_table_offset = XIP_BASE + 0x100;                     // set vector table offset
    asm("msr msp, %0" ::"r"(((void**)M0PLUS_REGS.vector_table_offset)[0])); // set stack pointer
    asm("bx %0" ::"r"(((void**)M0PLUS_REGS.vector_table_offset)[1]));       // jump to entry
}

namespace {
__attribute__((section(".boot2"))) auto wait_and_read(u8 count) -> u32 {
    while(!(SSI_REGS.status & ssi::Status::TFE) || SSI_REGS.status & ssi::Status::BUSY) {
    }
    u32 result = 0;
    while(count > 0) {
        result = SSI_REGS.data_register[0];
        count--;
    }
    return result;
}

__attribute__((section(".boot2"))) auto read_flash_sreg(u8 status_command) -> u8 {
    SSI_REGS.data_register[0] = status_command;
    SSI_REGS.data_register[0] = status_command;
    return wait_and_read(2);
}
} // namespace

__attribute__((section(".crc"))) u8 crc[4] = {'c', 'r', 'c', 'p'};
}
