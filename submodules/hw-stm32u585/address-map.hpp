#pragma once
// non-secure addresses (TrustZone disabled)
#define FLASH_BASE      0x0800'0000
#define SRAM_BASE       0x2000'0000
#define SPI2_BASE       0x4000'3800 // spi.hpp
#define FLASH_REGS_BASE 0x4002'2000 // flash.hpp
#define ICACHE_BASE     0x4003'0400 // TODO
#define GPIOA_BASE      0x4202'0000 // gpio.hpp
#define GPIOB_BASE      0x4202'0400 // gpio.hpp
#define GPIOC_BASE      0x4202'0800 // gpio.hpp
#define GPIOD_BASE      0x4202'0C00 // gpio.hpp
#define GPIOE_BASE      0x4202'1000 // gpio.hpp
#define GPIOF_BASE      0x4202'1400 // gpio.hpp
#define GPIOG_BASE      0x4202'1800 // gpio.hpp
#define GPIOH_BASE      0x4202'1C00 // gpio.hpp
#define GPIOI_BASE      0x4202'2000 // gpio.hpp
#define LPUART1_BASE    0x4600'2400 // usart.hpp
#define PWR_BASE        0x4602'0800 // pwr.hpp
#define RCC_BASE        0x4602'0C00 // rcc.hpp
#define PPB_BASE        0xE000'0000
#define SYSTICK_BASE    0xE000'E010 // systick.hpp
#define NVIC_BASE       0xE000'E100 // nvic.hpp
#define SCB_BASE        0xE000'ED00 // scb.hpp
#define DBGMCU_BASE     0xE004'4000 // dbgmcu.hpp
