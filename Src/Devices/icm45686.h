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
 * Bank 0 Registers & Constants
 * ========================================================================= */
#define ICM45686_REG_PWR_MGMT0          0x10
#define ICM45686_REG_INT1_CONFIG0       0x16
#define ICM45686_REG_INT1_CONFIG2       0x18
#define ICM45686_REG_INT1_STATUS0       0x19
#define ICM45686_REG_INT1_STATUS1       0x1A
#define ICM45686_REG_ACCEL_CONFIG0      0x1B
#define ICM45686_REG_GYRO_CONFIG0       0x1C
#define ICM45686_REG_ACCEL_DATA_X0_UI   0x00
#define ICM45686_REG_INTF_CONFIG1_OVRD  0x2D
#define ICM45686_REG_WHO_AM_I           0x72
#define ICM45686_REG_MISC2              0x7F

#define ICM45686_SPI_READ_BIT           0x80
#define ICM45686_WHO_AM_I_VAL           0xE9

#define ICM45686_INTF_CONFIG1_OVRD_4WIRE  0x0C
#define ICM45686_INT1_CONFIG0_DRDY_EN     0x04
#define ICM45686_INT1_CONFIG2_PP_PULSE_AH 0x01
#define ICM45686_ACCEL_CONFIG0_8G_200HZ   0x28
#define ICM45686_GYRO_CONFIG0_2000DPS_200HZ 0x18
#define ICM45686_PWR_MGMT0_ACCEL_GYRO_LN  0x0F

#define ICM45686_INT1_STATUS0_DRDY_MASK      (1U << 2)

/* =========================================================================
 * Data Types & Configuration
 * ========================================================================= */

typedef struct {
    int16_t accel[3]; /* [0]=X, [1]=Y, [2]=Z */
    int16_t gyro[3];  /* [0]=X, [1]=Y, [2]=Z */
    int16_t temp;
} ICM45686_Data_t;

/* Structure for injecting hardware dependencies (Dependency Injection) */
typedef struct {
    SPI_TypeDef          *SPIx;
    BDMA_Channel_TypeDef *BDMA_Tx;
    BDMA_Channel_TypeDef *BDMA_Rx;
    void (*RxCompleteCb)(ICM45686_Data_t *data); /* Callback to pass data up */
} ICM45686_HwConfig_t;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */
void    ICM45686_Init(const ICM45686_HwConfig_t *hw_config);
void    ICM45686_WriteRegister(uint8_t reg, uint8_t value);
void    ICM45686_Config(void);
bool    ICM45686_IsDataReady(void);
void    ICM45686_ReadDataBurst(ICM45686_Data_t *data);
void    ICM45686_StartDMAReadBurst(void);
void    ICM45686_DMA_RxComplete_Callback(void);