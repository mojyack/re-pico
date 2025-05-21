#pragma once
#include "address-map.hpp"
#include "int.hpp"

namespace m0plus {
struct SystickControlAndStatus {
    enum : u32 {
        ENABLE    = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        TICKINT   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        CLKSOURCE = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        COUNTFLAG = 0b0000'0000'0000'0001'0000'0000'0000'0000,
    };
};

struct SystickCalibrationValue {
    enum : u32 {
        TEMNS = 0b0000'0000'1111'1111'1111'1111'1111'1111,
        SKEW  = 0b0100'0000'0000'0000'0000'0000'0000'0000,
        NOREF = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct CPUID {
    enum : u32 {
        REVISION     = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        PARTNO       = 0b0000'0000'0000'0000'1111'1111'1111'0000,
        ARCHITECTURE = 0b0000'0000'0000'1111'0000'0000'0000'0000,
        VARIANT      = 0b0000'0000'1111'0000'0000'0000'0000'0000,
        IMPLEMENTER  = 0b1111'1111'0000'0000'0000'0000'0000'0000,
    };
};

struct IntControlAndState {
    enum : u32 {
        VECTACTIVE  = 0b0000'0000'0000'0000'0000'0001'1111'1111,
        VECTPENDING = 0b0000'0000'0001'1111'1111'0000'0000'0000,
        ISRPENDING  = 0b0000'0000'0100'0000'0000'0000'0000'0000,
        ISRPREEMPT  = 0b0000'0000'1000'0000'0000'0000'0000'0000,
        PENDSTCLR   = 0b0000'0010'0000'0000'0000'0000'0000'0000,
        PENDSTSET   = 0b0000'0100'0000'0000'0000'0000'0000'0000,
        PENDSVCLR   = 0b0000'1000'0000'0000'0000'0000'0000'0000,
        PENDSVSET   = 0b0001'0000'0000'0000'0000'0000'0000'0000,
        NMIPENDSET  = 0b1000'0000'0000'0000'0000'0000'0000'0000,
    };
};

struct AppIntAndResetControl {
    enum : u32 {
        VECTCLRACTIVE = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SYSRESETREQ   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        ENDIANESS     = 0b0000'0000'0000'0000'1000'0000'0000'0000,
        VECTKEY       = 0b1111'1111'1111'1111'0000'0000'0000'0000,
    };
};

struct SystemControl {
    enum : u32 {
        SLEEPONEXIT = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        SLEEPDEEP   = 0b0000'0000'0000'0000'0000'0000'0000'0100,
        SEVONPEND   = 0b0000'0000'0000'0000'0000'0000'0001'0000,
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
        SEPARATE = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        DREGION  = 0b0000'0000'0000'0000'1111'1111'0000'0000,
        IREGION  = 0b0000'0000'1111'1111'0000'0000'0000'0000,
    };
};

struct Control {
    enum : u32 {
        ENABLE     = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        HFNMIENA   = 0b0000'0000'0000'0000'0000'0000'0000'0010,
        PRIVDEFENA = 0b0000'0000'0000'0000'0000'0000'0000'0100,
    };
};

struct MpuRegionBaseAddress {
    enum : u32 {
        REGION = 0b0000'0000'0000'0000'0000'0000'0000'1111,
        VALID  = 0b0000'0000'0000'0000'0000'0000'0001'0000,
        ADDR   = 0b1111'1111'1111'1111'1111'1111'0000'0000,
    };
};

struct MpuRegionAttrAndSize {
    enum : u32 {
        ENABLE = 0b0000'0000'0000'0000'0000'0000'0000'0001,
        SIZE   = 0b0000'0000'0000'0000'0000'0000'0011'1110,
        SRD    = 0b0000'0000'0000'0000'1111'1111'0000'0000,
        ATTRS  = 0b1111'1111'1111'1111'0000'0000'0000'0000,
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
