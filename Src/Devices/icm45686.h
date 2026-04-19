/*
 * icm45686.h
 *
 *  Created on: Mar 29, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h723xx.h"
#include <stdint.h>
#include <stdbool.h>

/* =========================================================================
 * Bank 0 Registers
 * ========================================================================= */

/* Power supply / operating mode */
#define ICM45686_REG_PWR_MGMT0          0x10

/* INT1 interrupt configuration */
#define ICM45686_REG_INT1_CONFIG0       0x16  /* Masks enabling INT1 status flags             */
#define ICM45686_REG_INT1_CONFIG2       0x18  /* INT1 pin mode: polarity / push-pull / pulse */

/* INT1 status registers (R/C type - reading clears flags) */
#define ICM45686_REG_INT1_STATUS0       0x19  /* DRDY, AUX1_DRDY, FIFO_THS, FIFO_FULL, ...   */
#define ICM45686_REG_INT1_STATUS1       0x1A  /* WOM_X/Y/Z, APEX, ...                         */

/* Sensor configuration */
#define ICM45686_REG_ACCEL_CONFIG0      0x1B  /* ACCEL_UI_FS_SEL + ACCEL_ODR                  */
#define ICM45686_REG_GYRO_CONFIG0       0x1C  /* GYRO_UI_FS_SEL  + GYRO_ODR                   */

/* Data registers (burst read from TEMP_DATA1) */
#define ICM45686_REG_ACCEL_DATA_X0_UI   0x00  /* Burst read start address */

/* Interface configuration */
#define ICM45686_REG_INTF_CONFIG1_OVRD  0x2D  /* Override SPI 3/4-wire mode                  */

/* Identyfikacja */
#define ICM45686_REG_WHO_AM_I           0x72
#define ICM45686_REG_MISC2              0x7F  /* SOFT_RST                                      */

/* =========================================================================
 * SPI Constants
 * ========================================================================= */
#define ICM45686_SPI_READ_BIT           0x80
#define ICM45686_WHO_AM_I_VAL           0xE9

/* =========================================================================
 * Configuration Values
 * ========================================================================= */

/*
 * INTF_CONFIG1_OVRD (0x2D) = 0x0C
 *   bit 3: AP_SPI_34_MODE_OVRD     = 1  (enable override of 3/4-wire mode)
 *   bit 2: AP_SPI_34_MODE_OVRD_VAL = 1  (select 4-wire)
 *   bit 1: AP_SPI_MODE_OVRD        = 0  (don't override SPI mode 0/1/2/3)
 *   bit 0: AP_SPI_MODE_OVRD_VAL    = 0  (SPI mode 0 or 3 - default)
 */
#define ICM45686_INTF_CONFIG1_OVRD_4WIRE  0x0C

/*
 * INT1_CONFIG0 (0x16) = 0x04
 *   bit 2: INT1_STATUS_EN_DRDY = 1  (enable "UI Data Ready" flag in INT1_STATUS0)
 */
#define ICM45686_INT1_CONFIG0_DRDY_EN     0x04

/*
 * INT1_CONFIG2 (0x18) = 0x01
 *   bit 2: INT1_DRIVE    = 0  (push-pull)
 *   bit 1: INT1_MODE     = 0  (pulse mode - auto-reset after ODR)
 *   bit 0: INT1_POLARITY = 1  (active-high, consistent with EXTI rising-edge)
 */
#define ICM45686_INT1_CONFIG2_PP_PULSE_AH 0x01

/*
 * ACCEL_CONFIG0 (0x1B)
 *   bits 6:4 ACCEL_UI_FS_SEL = 010 => +/-8g
 *   bits 3:0 ACCEL_ODR       = 1000 => 200 Hz (LN or LP)
 */
#define ICM45686_ACCEL_CONFIG0_8G_200HZ   0x28

/*
 * GYRO_CONFIG0 (0x1C)
 *   bits 7:4 GYRO_UI_FS_SEL = 0001 => +/-2000 dps
 *   bits 3:0 GYRO_ODR       = 1000 => 200 Hz (LN)
 */
#define ICM45686_GYRO_CONFIG0_2000DPS_200HZ 0x18

/*
 * PWR_MGMT0 (0x10) = 0x0F
 *   bits 3:2 GYRO_MODE  = 11 => Low Noise
 *   bits 1:0 ACCEL_MODE = 11 => Low Noise
 */
#define ICM45686_PWR_MGMT0_ACCEL_GYRO_LN 0x0F

/* =========================================================================
 * Bit masks for INT1_STATUS0 (0x19)
 * ========================================================================= */
#define ICM45686_INT1_STATUS0_DRDY_MASK      (1U << 2)  /* UI Data Ready          */
#define ICM45686_INT1_STATUS0_AUX1_DRDY_MASK (1U << 3)  /* AUX1 Data Ready        */
#define ICM45686_INT1_STATUS0_FIFO_THS_MASK  (1U << 1)  /* FIFO threshold reached */
#define ICM45686_INT1_STATUS0_FIFO_FULL_MASK (1U << 0)  /* FIFO full              */

/* =========================================================================
 * Data Types
 * ========================================================================= */
typedef struct {
    int16_t accel[3]; /* [0]=X, [1]=Y, [2]=Z */
    int16_t gyro[3];  /* [0]=X, [1]=Y, [2]=Z */
    int16_t temp;
} ICM45686_Data_t;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */
void    ICM45686_Init(void);
uint8_t ICM45686_ReadWhoAmI(void);
void    ICM45686_WriteRegister(uint8_t reg, uint8_t value);
void    ICM45686_Config(void);
bool    ICM45686_IsDataReady(void);
void    ICM45686_ReadDataBurst(ICM45686_Data_t *data);