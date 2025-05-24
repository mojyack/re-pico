#pragma once
#include <noxx/int.hpp>

#include "address-map.hpp"

namespace ssi {
struct Control0TransferMode {
    enum : u32 {
        TXAndRX    = 0,
        TXOnly     = 1,
        RXOnly     = 2,
        EEPROMRead = 3,
    };
};

struct Control0SPIFrameFormat {
    enum : u32 {
        Std  = 0,
        Dual = 1,
        Quad = 2,
    };
};

struct Control0 {
    enum : u32 {
        DataFrameSize           = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        FrameFormat             = 0b0000'0000'0000'0000'0000'0000'0011'0000,
        SerialClockPhase        = 0b0000'0000'0000'0000'0000'0000'0100'0000,
        SerialClockPolarity     = 0b0000'0000'0000'0000'0000'0000'1000'0000,
        TransferMode            = 0b0000'0000'0000'0000'0000'0011'0000'0000,
        SlaveOutputEnable       = 0b0000'0000'0000'0000'0000'0100'0000'0000,
        ShiftRegisterLoop       = 0b0000'0000'0000'0000'0000'1000'0000'0000,
        ControlFrameSize        = 0b0000'0000'0000'0000'1111'0000'0000'0000,
        DataFrameSize32         = 0b0000'0000'0001'1111'0000'0000'0000'0000,
        SPIFrameFormat          = 0b0000'0000'0110'0000'0000'0000'0000'0000,
        SlaveSelectToggleEnable = 0b0000'0001'0000'0000'0000'0000'0000'0000,
    };
};

struct MicrowireControl {
    enum : u32 {
        TransferMode = 0,
        Control      = 1,
        Handshaking  = 2,
    };
};

struct Status {
    enum : u32 {
        SSIBusy            = 0b0000'0000'0000'0000'0000'0000'0000'0001, // ssi busy flag
        TXFIFONotFull      = 0b0000'0000'0000'0000'0000'0000'0000'0010, // transmit fifo not full
        TXFIFOEmpty        = 0b0000'0000'0000'0000'0000'0000'0000'0100, // transmit fifo empty
        RXFIFONotFull      = 0b0000'0000'0000'0000'0000'0000'0000'1000, // receive fifo not full
        RXFIFOFull         = 0b0000'0000'0000'0000'0000'0000'0001'0000, // receive fifo full
        TXErorr            = 0b0000'0000'0000'0000'0000'0000'0010'0000, // transmission error
        DataCollisionError = 0b0000'0000'0000'0000'0000'0000'0100'0000, // data collision error
    };
};

// for interrupt_mask, interrupt_status, raw_intrerrupt_status
struct Interrupts {
    enum : u32 {
        TXFIFOEmpty           = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        TXFIFOOverflow        = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        RXFIFOUnderflow       = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        RXFIFOOverflow        = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        RXFIFOFull            = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        MultiMasterContention = 0b0000'0000'0000'0000'0000'0000'0010'0000,
    };
};

struct DMAControl {
    enum : u32 {
        TXDMAEnable = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        RXDMAEnable = 0b0000'0000'0000'0000'0000'0000'0000'0010,
    };
};

struct SPIControl0TransType {
    enum : u32 {
        _1C1A = 0,
        _1C2A = 1,
        _2C2A = 2,
    };
};

struct SPIControl0InstructionLength {
    enum : u32 {
        _None = 0,
        _4B   = 1,
        _8B   = 2,
        _16B  = 3,
    };
};

struct SPIControl0 {
    enum : u32 {
        TransType               = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        AddressLength           = 0b0000'0000'0000'0000'0000'0000'0011'1100,
        InstructionLength       = 0b0000'0000'0000'0000'0000'0011'0000'0000,
        WaitCycles              = 0b0000'0000'0000'0000'1111'1000'0000'0000,
        SPIDDREnable            = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        InstructionDDREnable    = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        SPIReadDataStrobeEnable = 0b0000'0000'0000'0100'0000'0000'0000'0000,
        XIPCommand              = 0b1111'1111'0000'0000'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  control0;                          // ctrlr0
    v32  control1;                          // ctrlr1
    v32  ssi_enable;                        // ssienr
    v32  microwire_control;                 // mwcr
    v32  slave_enable;                      // ser
    v32  baud_rate;                         // baudr
    v32  tx_fifo_threahold_level;           // txftlr
    v32  rx_fifo_threahold_level;           // rxftlr
    cv32 tx_fifo_level;                     // txflr
    cv32 rx_fifo_level;                     // rxflr
    cv32 status;                            // sr
    v32  interrupt_mask;                    // imr
    cv32 interrupt_status;                  // isr
    cv32 raw_intrerrupt_status;             // risr
    cv32 tx_fifo_overflow_interrupt_clear;  // txoicr
    cv32 rx_fifo_overflow_interrupt_clear;  // rxoicr
    cv32 rx_fifo_underflow_interrupt_clear; // rxuicr
    cv32 multi_master_interrupt_clear;      // msticr
    cv32 interrupt_clear;                   // icr
    v32  dma_control;                       // dmacr
    v32  dma_tx_data_level;                 // dmatdlr
    v32  dma_rx_data_level;                 // dmardlr
    cv32 identification;                    // idr
    cv32 version_id;                        // ssi_version_id
    v32  data_register[36];                 // dr0
    v32  rx_sample_delay;                   // rx_sample_dly
    v32  spi_control0;                      // spi_ctrlr0
    v32  tx_drive_edge;                     // txd_drive_edge
};
} // namespace ssi

#define SSI_REGS (*(ssi::Regs*)(XIP_SSI_BASE))
