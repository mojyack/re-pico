#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace m0plus {
struct SystickControlAndStatus {
    enum : u32 {
        Enable        = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        TickInterrupt = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        ClockSource   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        CountFlag     = 0b0000'0000'0000'0001'0000'0000'0000'0000,
    };
};

struct SystickCalibrationValue {
    enum : u32 {
        TenMS = 0b0000'0000'1111'1111'1111'1111'1111'1111,
        Skew  = 0b0100'0000'0000'0000'0000'0000'0000'0000,
        NoRef = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct CPUID {
    enum : u32 {
        Revision     = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        PartNum      = 0b0000'0000'0000'0000'1111'1111'1111'0000,
        Architecture = 0b0000'0000'0000'1111'0000'0000'0000'0000,
        Variant      = 0b0000'0000'1111'0000'0000'0000'0000'0000,
        Implementer  = 0b1111'1111'0000'0000'0000'0000'0000'0000,
    };
};

struct IntControlAndState {
    enum : u32 {
        VECTActive     = 0b0000'0000'0000'0000'0000'0001'1111'1111,
        VECTPending    = 0b0000'0000'0001'1111'1111'0000'0000'0000,
        ISRPending     = 0b0000'0000'0100'0000'0000'0000'0000'0000,
        ISRPreempt     = 0b0000'0000'1000'0000'0000'0000'0000'0000,
        PendStateClear = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PendStateSet   = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        PendSVClear    = 0b0000'1000'0000'0000'0000'0000'0000'0000,
        PendSVSet      = 0b0001'0000'0000'0000'0000'0000'0000'0000,
        NMIPendSet     = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct AppIntAndResetControl {
    enum : u32 {
        VECTClearActive = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SysResetReq     = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        Endianess       = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        VECTKey         = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

struct SystemControl {
    enum : u32 {
        SleepOnExit     = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SleepDeep       = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SendEventOnPend = 0b0000'0000'0000'0000'0000'0000'0001'0000,
    };
};

struct ConfigrationAndControl {
    enum : u32 {
        UNALIGN_TRP = 0b0000'0000'0000'0000'0000'0000'0000'1000,
        STKALIGN    = 0b0000'0000'0000'0000'0000'0010'0000'0000,
    };
};

struct MPUType {
    enum : u32 {
        Separate           = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        NumberOfRegions    = 0b0000'0000'0000'0000'1111'1111'0000'0000,
        InstructionRegions = 0b0000'0000'1111'1111'0000'0000'0000'0000,
    };
};

struct Control {
    enum : u32 {
        ENABLE     = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        HFNMIENA   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PRIVDEFENA = 0b0000'0000'0000'0000'0000'0000'0000'0100,
    };
};

struct MPURegionBaseAddress {
    enum : u32 {
        Region = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        Valid  = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        Addr   = 0b1111'1111'1111'1111'1111'1111'0000'0000,
    };
};

struct MPURegionAttrAndSize {
    enum : u32 {
        Enable           = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        Size             = 0b0000'0000'0000'0000'0000'0000'0011'1110,
        SubRegionDisable = 0b0000'0000'0000'0000'1111'1111'0000'0000,
        Attrs            = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

struct Regs {
    cv32 reserved1[14340];                 //
    v32  systick_control_and_status;       // SYST_CSR
    v32  systick_reload_value;             // SYST_RVR
    v32  systick_current_value;            // SYST_CVR
    cv32 systick_calibration_value;        // SYST_CALIB
    cv32 reserved2[56];                    //
    v32  nvic_set_enable;                  // NVIC_ISER
    cv32 reserved3[31];                    //
    v32  nvic_clear_enable;                // NVIC_ICER
    cv32 reserved4[31];                    //
    v32  nvic_set_pending;                 // NVIC_ISPR
    cv32 reserved5[31];                    //
    v32  nvic_clear_pending;               // NVIC_ICPR
    cv32 reserved6[95];                    //
    v32  nvic_priority[8];                 // NVIC_IPR0..7
    cv32 reserved7[568];                   //
    cv32 cpuid;                            // CPUID
    v32  int_control_and_state;            // ICSR
    v32  vector_table_offset;              // VTOR
    v32  app_int_and_reset_control;        // AIRCR
    v32  system_control;                   // SCR
    v32  configration_and_control;         // CCR
    cv32 reserved8[1];                     //
    v32  system_handler_priority_2;        // SHPR2
    v32  system_handler_priority_3;        // SHPR3
    v32  system_handler_control_and_state; // SHCSR
    cv32 reserved9[26];                    //
    cv32 mpu_type;                         // MPU_TYPE
    v32  mpu_control;                      // MPU_CTRL
    v32  mpu_region_number;                // MPU_RNR
    v32  mpu_region_base_address;          // MPU_RBAR
    v32  mpu_region_attr_and_size;         // MPU_RASR
};
} // namespace m0plus

#define M0PLUS_REGS (*(m0plus::Regs*)(PPB_BASE))
