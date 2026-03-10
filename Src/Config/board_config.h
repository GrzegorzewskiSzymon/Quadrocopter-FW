/*
 * board_config.h
 *
 *  Created on: Mar 10, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h7xx.h"


/* ========================================================================= */
/* DRONE HARDWARE PIN MAP PCBv0.1 - UNIFIED PORT/PIN MACROS                     */
/* ========================================================================= */

/* --- LED STATUS INDICATORS --- */
#define LED_SCK             GPIOE, 2U
#define LED_MOSI            GPIOE, 6U
#define LED_RED             GPIOA, 10U
#define LED_YELLOW          GPIOA, 9U
#define LED_GREEN           GPIOA, 8U
#define LED_BLUE            GPIOC, 9U

/* --- ESC PWM OUTPUTS (6-PWM MODE) --- */
#define ESC1_INHA           GPIOD, 14U
#define ESC1_INHB           GPIOD, 13U
#define ESC1_INHC           GPIOD, 12U
#define ESC1_INLA           GPIOB, 15U
#define ESC1_INLB        	GPIOD, 8U
#define ESC1_INLC        	GPIOD, 10U

// ESC 2
#define ESC2_INHA           GPIOC, 6U
#define ESC2_INHB           GPIOC, 7U
#define ESC2_INHC           GPIOC, 8U
#define ESC2_INLA           GPIOG, 5U
#define ESC2_INLB           GPIOG, 4U
#define ESC2_INLC           GPIOG, 3U

// ESC 3
#define ESC3_INHA           GPIOF, 2U
#define ESC3_INHB           GPIOF, 1U
#define ESC3_INHC           GPIOF, 0U
#define ESC3_INLA           GPIOE, 13U
#define ESC3_INLB           GPIOE, 14U
#define ESC3_INLC           GPIOE, 15U

// ESC 4
#define ESC4_INHA           GPIOA, 2U
#define ESC4_INHB           GPIOA, 1U
#define ESC4_INHC           GPIOA, 0U
#define ESC4_INLA           GPIOE, 8U
#define ESC4_INLB           GPIOE, 9U
#define ESC4_INLC           GPIOE, 10U

/* --- ESC CONTROL & POWER BOARD SPI --- */
#define PB_ESC_EN           GPIOF, 13U
#define PB_ESC_SCK          GPIOB, 13U
#define PB_ESC_MISO         GPIOC, 2U  // PC2_C
#define PB_ESC_MOSI         GPIOC, 3U  // PC3_C
#define PB_ESC_NFAULT       GPIOC, 5U

#define PB_ESC_NSCS1        GPIOB, 12U
#define PB_ESC_NSCS2        GPIOD, 11U
#define PB_ESC_NSCS3        GPIOE, 12U
#define PB_ESC_NSCS4        GPIOE, 7U

/* --- IMU 1 (SPI1) --- */
#define IMU1_CS             GPIOG, 10U
#define IMU1_SCK            GPIOG, 11U
#define IMU1_MISO           GPIOB, 4U
#define IMU1_MOSI           GPIOD, 7U
#define IMU1_INT            GPIOD, 3U
#define IMU_FSYNC           GPIOD, 5U

/* --- IMU 2 (SPI6) --- */
#define IMU2_CS             GPIOC, 4U
#define IMU2_SCK            GPIOA, 5U
#define IMU2_MISO           GPIOG, 12U
#define IMU2_MOSI           GPIOA, 7U
#define IMU2_INT            GPIOA, 4U

/* --- MAGNETOMETER (SPI5) --- */
#define MAG_CS              GPIOF, 6U
#define MAG_SCK             GPIOF, 7U
#define MAG_MISO            GPIOF, 8U
#define MAG_MOSI            GPIOF, 9U
#define MAG_INT             GPIOF, 10U

/* --- BAROMETER (I2C4) --- */
#define BARO_SCL            GPIOF, 14U
#define BARO_SDA            GPIOF, 15U
#define BARO_INT            GPIOB, 14U

/* --- TOF SENSOR (I2C1) --- */
#define TOF_SCL             GPIOB, 6U
#define TOF_SDA             GPIOB, 7U
#define TOF_EN              GPIOB, 5U
#define TOF_INT             GPIOE, 0U
#define PB_TOF_INT          GPIOG, 2U

/* --- GPS (UART9) --- */
#define GPS_RX              GPIOG, 0U
#define GPS_TX              GPIOG, 1U
#define GPS_SYS_RSTn        GPIOF, 11U
#define GPS_WAKE_UP         GPIOF, 12U

/* --- RF MODULE NRF24L01 --- */
#define RF_CE               GPIOA, 15U
#define RF_CSN              GPIOD, 0U
#define RF_SCK              GPIOC, 10U
#define RF_MISO             GPIOC, 11U
#define RF_MOSI             GPIOC, 12U
#define RF_IRQ              GPIOD, 1U
#define RF_TXEN             GPIOD, 2U
#define RF_RXEN             GPIOD, 4U

/* --- EXTERNAL FLASH MEMORY --- */
#define FLASH_CS            GPIOE, 11U
#define FLASH_SCK           GPIOB, 2U
#define FLASH_IO0           GPIOB, 1U
#define FLASH_IO1           GPIOB, 0U
#define FLASH_IO2           GPIOA, 3U
#define FLASH_IO3           GPIOA, 6U

/* --- POWER MANAGEMENT & BMS --- */
#define PB_CB_ALERT         GPIOG, 8U
#define PB_BMS_CHRG_OK      GPIOG, 7U
#define PB_BMS_PROCHOT      GPIOD, 15U
#define PB_BMS_OTG_VAP      GPIOD, 9U

/* --- DEBUG & USB --- */
#define SWDIO               GPIOA, 13U
#define SWCLK               GPIOA, 14U
#define SWO                 GPIOB, 3U
#define USB_DP              GPIOA, 12U
#define USB_DM              GPIOA, 11U

/* --- MISC I2C (PB_SCL / PB_SDA) --- */
#define PB_SCL              GPIOB, 10U
#define PB_SDA              GPIOB, 11U


