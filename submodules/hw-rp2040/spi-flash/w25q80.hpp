#pragma once
#include <noxx/int.hpp>

namespace flash {
struct Instructions {
    enum : u8 {
        // standard
        WriteEnable              = 0x06,
        VolatileSRWriteEnable    = 0x50,
        WriteDisable             = 0x04,
        ReadStatusRegister1      = 0x05,
        ReadStatusRegister2      = 0x35,
        WriteStatusRegister      = 0x01,
        PageProgram              = 0x02,
        SectorErase4K            = 0x20,
        BlockErase32K            = 0x52,
        BlockErase64K            = 0xd8,
        ChipErase                = 0xc7,
        EraseProgramSuspend      = 0x75,
        EraseProgramResume       = 0x7a,
        PowerDown                = 0xb9,
        ReadData                 = 0x03,
        FastRead                 = 0x0b,
        ReleasePowerdown         = 0xab,
        Manufacture              = 0x90,
        JedecID                  = 0x9f,
        ReadUniqueID             = 0x4b,
        ReadSFDPRegister         = 0x5a,
        EraseSecurityRegisters   = 0x44,
        ProgramSecurityRegisters = 0x42,
        ReadSecurityRegisters    = 0x48,
        EnableReset              = 0x66,
        Reset                    = 0x99,
        // dual
        FastReadDualOutput  = 0x3b,
        FastReadDualIO      = 0xbb,
        ManufacturebyDualIO = 0x92,
        // quad
        QuadPageProgram     = 0x32,
        FastReadQuadOutput  = 0x6b,
        FastReadQuadIO      = 0xeb,
        SetBurstWithWrap    = 0x77,
        ManufactureByQuadIO = 0x94,
    };
};

struct StatusRegister1 {
    enum : u8 {
        BUSY = 0b0000'0001,
        WEL  = 0b0000'0010, // write enable latch
        BP   = 0b0001'1100, // block protect
        TB   = 0b0010'0000, // top/bottom protect
        SEC  = 0b0100'0000, // sector prortect
        SRP0 = 0b1000'0000, // status register protect0
    };
};

struct StatusRegister2 {
    enum : u8 {
        SRP1 = 0b0000'0001, // status register protect1
        QE   = 0b0000'0010, // quad enabled
        LB   = 0b0011'1000, // security register lock
        CMP  = 0b0100'0000, // complement protect
        SUS  = 0b1000'0000, // suspend status
    };
};

struct ReadMode {
    enum : u8 {
        Continuous = 0xa0,
    };
};
} // namespace flash
