#pragma once
#define ROM_BASE                 0x0000'0000
#define XIP_BASE                 0x1000'0000
#define XIP_NOALLOC_BASE         0x1100'0000
#define XIP_NOCACHE_BASE         0x1200'0000
#define XIP_NOCACHE_NOALLOC_BASE 0x1300'0000
#define XIP_CTRL_BASE            0x1400'0000
#define XIP_SRAM_BASE            0x1500'0000
#define XIP_SRAM_END             0x1500'4000
#define XIP_SSI_BASE             0x1800'0000 // ssi.hpp
#define SRAM_BASE                0x2000'0000
#define SRAM_STRIPED_BASE        0x2000'0000
#define SRAM_STRIPED_END         0x2004'0000
#define SRAM4_BASE               0x2004'0000
#define SRAM5_BASE               0x2004'1000
#define SRAM_END                 0x2004'2000
#define SRAM0_BASE               0x2100'0000
#define SRAM1_BASE               0x2101'0000
#define SRAM2_BASE               0x2102'0000
#define SRAM3_BASE               0x2103'0000
#define SYSINFO_BASE             0x4000'0000 // sysinfo.hpp
#define SYSCFG_BASE              0x4000'4000 // syscfg.hpp
#define CLOCKS_BASE              0x4000'8000 // clocks.hpp
#define RESETS_BASE              0x4000'c000 // resets.hpp
#define PSM_BASE                 0x4001'0000 // psm.hpp
#define IO_BANK0_BASE            0x4001'4000 // io-bank0.hpp
#define IO_QSPI_BASE             0x4001'8000 // io-qspi.hpp
#define PADS_BANK0_BASE          0x4001'c000 // pads.hpp
#define PADS_QSPI_BASE           0x4002'0000 // pads.hpp
#define XOSC_BASE                0x4002'4000 // xosc.hpp
#define PLL_SYS_BASE             0x4002'8000 // pll.hpp
#define PLL_USB_BASE             0x4002'c000 // pll.hpp
#define BUSCTRL_BASE             0x4003'0000 // TODO
#define UART0_BASE               0x4003'4000 // TODO
#define UART1_BASE               0x4003'8000 // TODO
#define SPI0_BASE                0x4003'c000 // TODO
#define SPI1_BASE                0x4004'0000 // TODO
#define I2C0_BASE                0x4004'4000 // TODO
#define I2C1_BASE                0x4004'8000 // TODO
#define ADC_BASE                 0x4004'c000 // TODO
#define PWM_BASE                 0x4005'0000 // TODO
#define TIMER_BASE               0x4005'4000 // timer.hpp
#define WATCHDOG_BASE            0x4005'8000 // wd.hpp
#define RTC_BASE                 0x4005'c000 // TODO
#define ROSC_BASE                0x4006'0000 // rosc.hpp
#define VREG_AND_CHIP_RESET_BASE 0x4006'4000 // TODO
#define TBMAN_BASE               0x4006'c000 // TODO
#define DMA_BASE                 0x5000'0000 // TODO
#define USBCTRL_BASE             0x5010'0000 // TODO
#define USBCTRL_DPRAM_BASE       0x5010'0000 // TODO
#define USBCTRL_REGS_BASE        0x5011'0000 // TODO
#define PIO0_BASE                0x5020'0000 // TODO
#define PIO1_BASE                0x5030'0000 // TODO
#define XIP_AUX_BASE             0x5040'0000 // TODO
#define SIO_BASE                 0xd000'0000 // sio.hpp
#define PPB_BASE                 0xe000'0000 // m0plus.hpp
