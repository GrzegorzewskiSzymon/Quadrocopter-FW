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
#include "status.h"

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

/* ODR is 200 Hz (5 ms); four missed samples in a row mean the data path is dead */
#define ICM45686_SAMPLE_TIMEOUT_MS      20U

/* A 15-byte burst takes ~30 us; with a 1 ms tick, 2 ticks guarantee at least 1 ms elapsed */
#define ICM45686_BURST_TIMEOUT_MS       2U

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
    void (*BusRecoverCb)(void);                  /* Re-initialises the SPI peripheral after a stalled burst */
} ICM45686_HwConfig_t;

/* =========================================================================
 * Function Declarations
 * ========================================================================= */
Status_t ICM45686_Init(const ICM45686_HwConfig_t *hw_config);
void     ICM45686_WriteRegister(uint8_t reg, uint8_t value);
void     ICM45686_Config(void);
bool     ICM45686_IsDataReady(void);
void     ICM45686_ReadDataBurst(ICM45686_Data_t *data);
void     ICM45686_StartDMAReadBurst(void);
void     ICM45686_DMA_RxComplete_Callback(void);
void     ICM45686_DMA_Error_Callback(void);

/* False until Init succeeds, after a DMA error, or when samples stop arriving */
bool     ICM45686_IsHealthy(void);
/* Call periodically (<= 1 ms) from thread context: aborts a burst older than
   ICM45686_BURST_TIMEOUT_MS and re-initialises the bus */
void     ICM45686_CheckStall(void);
/* Bursts aborted by ICM45686_CheckStall() */
uint32_t ICM45686_GetStalledTransfers(void);