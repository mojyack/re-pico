#include "bits.hpp"
#include "m0plus.hpp"
#include "spi-flash/w25q80.hpp"
#include "ssi.hpp"

extern "C" {
namespace {
auto wait_and_read(u8 count) -> u32;
auto read_flash_sreg(u8 status_command) -> u8;
} // namespace

__attribute__((section(".boot2"))) auto boot_stage2() -> void {
    SSI_REGS.ssi_enable = 0;
    SSI_REGS.baud_rate  = 4;
    SSI_REGS.control0 =
        BF(ssi::Control0::TransferMode, ssi::Control0TransferMode::TXAndRX) |
        BF(ssi::Control0::DataFrameSize32, 7);
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
        BF(ssi::Control0::TransferMode, ssi::Control0TransferMode::EEPROMRead) |
        BF(ssi::Control0::DataFrameSize32, 31) |
        BF(ssi::Control0::SPIFrameFormat, ssi::Control0SPIFrameFormat::Quad);
    SSI_REGS.spi_control0 =
        BF(ssi::SPIControl0::AddressLength, 8 /*32-bits*/) |
        BF(ssi::SPIControl0::InstructionLength, ssi::SPIControl0InstructionLength::_8B) |
        BF(ssi::SPIControl0::TransType, ssi::SPIControl0TransType::_1C2A) |
        BF(ssi::SPIControl0::WaitCycles, 4);
    SSI_REGS.ssi_enable = 1;

    SSI_REGS.data_register[0] = flash::Instructions::FastReadQuadIO;
    SSI_REGS.data_register[0] = flash::ReadMode::Continuous;
    wait_and_read(2);

    // configure
    SSI_REGS.ssi_enable = 0;
    SSI_REGS.spi_control0 =
        BF(ssi::SPIControl0::XIPCommand, flash::ReadMode::Continuous) |
        BF(ssi::SPIControl0::AddressLength, 8 /*32-bits*/) |
        BF(ssi::SPIControl0::WaitCycles, 4) |
        BF(ssi::SPIControl0::InstructionLength, ssi::SPIControl0InstructionLength::_None) |
        BF(ssi::SPIControl0::TransType, ssi::SPIControl0TransType::_2C2A);
    SSI_REGS.ssi_enable = 1;

    M0PLUS_REGS.vector_table_offset = XIP_BASE + 0x100;                     // set vector table offset
    asm("msr msp, %0" ::"r"(((void**)M0PLUS_REGS.vector_table_offset)[0])); // set stack pointer
    asm("bx %0" ::"r"(((void**)M0PLUS_REGS.vector_table_offset)[1]));       // jump to entry
}

namespace {
__attribute__((section(".boot2"))) auto wait_and_read(u8 count) -> u32 {
    while(!(SSI_REGS.status & ssi::Status::TXFIFOEmpty) || SSI_REGS.status & ssi::Status::SSIBusy) {
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
