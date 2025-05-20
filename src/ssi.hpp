#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace ssi {
struct Control0TransferMode {
    enum : u32 {
        TX_AND_RX   = 0,
        TX_ONLY     = 1,
        RX_ONLY     = 2,
        EEPROM_READ = 3,
    };
};

struct Control0SPIFrameFormat {
    enum : u32 {
        STD  = 0,
        DUAL = 1,
        QUAD = 2,
    };
};

struct Control0 {
    enum : u32 {
        DFS     = 0b0000'0000'0000'0000'0000'0000'0000'1111, // data frame size
        FRF     = 0b0000'0000'0000'0000'0000'0000'0011'0000, // frame format
        SCPH    = 0b0000'0000'0000'0000'0000'0000'0100'0000, // serial clock phase
        SCPOL   = 0b0000'0000'0000'0000'0000'0000'1000'0000, // serial clock polarity
        TMOD    = 0b0000'0000'0000'0000'0000'0011'0000'0000, // transfer mode
        SLV_OE  = 0b0000'0000'0000'0000'0000'0100'0000'0000, // slave output enable
        SRL     = 0b0000'0000'0000'0000'0000'1000'0000'0000, // shift register loop
        CFS     = 0b0000'0000'0000'0000'1111'0000'0000'0000, // control frame size
        DFS_32  = 0b0000'0000'0001'1111'0000'0000'0000'0000, // data frame size in 32b transfer mode
        SPI_FRF = 0b0000'0000'0110'0000'0000'0000'0000'0000, // spi frame format
        SSTE    = 0b0000'0001'0000'0000'0000'0000'0000'0000, // slave select toggle enable
    };
};

struct MicrowireControl {
    enum : u32 {
        MWMOD = 0, // microwire transfer mode
        MDD   = 1, // microwire control
        MHS   = 2, // microwire handshaking
    };
};

struct Status {
    enum : u32 {
        BUSY = 0b0000'0000'0000'0000'0000'0000'0000'0001, // ssi busy flag
        TFNF = 0b0000'0000'0000'0000'0000'0000'0000'0010, // transmit fifo not full
        TFE  = 0b0000'0000'0000'0000'0000'0000'0000'0100, // transmit fifo full
        RFNF = 0b0000'0000'0000'0000'0000'0000'0000'1000, // receive fifo not full
        RFE  = 0b0000'0000'0000'0000'0000'0000'0001'0000, // receive fifo full
        TXE  = 0b0000'0000'0000'0000'0000'0000'0010'0000, // transmission error
        DCOL = 0b0000'0000'0000'0000'0000'0000'0100'0000, // data collision error
    };
};

// for interrupt_mask, interrupt_status, raw_intrerrupt_status
struct Interrupts {
    enum : u32 {
        TXE = 0b0000'0000'0000'0000'0000'0000'0000'0001, // transmit fifo empty
        TXO = 0b0000'0000'0000'0000'0000'0000'0000'0010, // transmit fifo overflow
        RXU = 0b0000'0000'0000'0000'0000'0000'0000'0100, // receive fifo underflow
        RXO = 0b0000'0000'0000'0000'0000'0000'0000'1000, // receive fifo overflow
        RXF = 0b0000'0000'0000'0000'0000'0000'0001'0000, // receive fifo full
        MST = 0b0000'0000'0000'0000'0000'0000'0010'0000, // multi-master contention
    };
};

struct DMAControl {
    enum : u32 {
        TDMAE = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        RDMAE = 0b0000'0000'0000'0000'0000'0000'0000'0010,
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
        _NONE = 0,
        _4B   = 1,
        _8B   = 2,
        _16B  = 3,
    };
};

struct SPIControl0 {
    enum : u32 {
        TRANS_TYPE  = 0b0000'0000'0000'0000'0000'0000'0000'0011,
        ADDR_L      = 0b0000'0000'0000'0000'0000'0000'0011'1100,
        INST_L      = 0b0000'0000'0000'0000'0000'0011'0000'0000,
        WAIT_CYCLES = 0b0000'0000'0000'0000'1111'1000'0000'0000,
        SPI_DDR_EN  = 0b0000'0000'0000'0001'0000'0000'0000'0000,
        INST_DDR_EN = 0b0000'0000'0000'0010'0000'0000'0000'0000,
        SPI_RXDS_EN = 0b0000'0000'0000'0100'0000'0000'0000'0000,
        XIP_CMD     = 0b1111'1111'0000'0000'0000'0000'0000'0000,
    };
};

struct Regs {
    v32  control0;                         // ctrlr0
    v32  control1;                         // ctrlr1
    v32  ssi_enable;                       // ssienr
    v32  microwire_control;                // mwcr
    v32  slave_enable;                     // ser
    v32  baud_rate;                        // baudr
    v32  tx_fifo_threahold_level;          // txftlr
    v32  rx_fifo_threahold_level;          // rxftlr
    cv32 tx_fifo_level;                    // txflr
    cv32 rx_fifo_level;                    // rxflr
    cv32 status;                           // sr
    v32  interrupt_mask;                   // imr
    cv32 interrupt_status;                 // isr
    cv32 raw_intrerrupt_status;            // risr
    cv32 tx_fifo_overflow_interrupt_clear; // txoicr
    cv32 rx_fifo_overflow_interrupt_clear; // rxoicr
    cv32 multi_master_interrupt_clear;     // msticr
    cv32 interrupt_clear;                  // icr
    v32  dma_control;                      // dmacr
    v32  dma_tx_data_level;                // dmatdlr
    v32  dma_rx_data_level;                // dmardlr
    cv32 identification;                   // idr
    cv32 version_id;                       // ssi_version_id
    v32  data_register[36];                // dr0
    v32  rx_sample_delay;                  // rx_sample_dly
    v32  spi_control0;                     // spi_ctrlr0
    v32  tx_drive_edge;                    // txd_drive_edge
};
} // namespace ssi

#define SSI_REGS (*(ssi::Regs*)(XIP_SSI_BASE))
