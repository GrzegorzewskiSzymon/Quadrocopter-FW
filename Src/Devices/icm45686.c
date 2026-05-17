/*
 * icm45686.c
 *
 *  Created on: Mar 29, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "icm45686.h"
#include "board_config.h"
#include "spi.h"
#include "systick.h"
#include "gpio.h"
#include "stddef.h"

/* CRITICAL: 32-byte alignment for D-Cache line boundary. Must be placed in D3 Domain (SRAM4) */
__attribute__((aligned(32), section(".sram4"))) static uint8_t imu_tx_buf[32];
__attribute__((aligned(32), section(".sram4"))) static uint8_t imu_rx_buf[32];

static ICM45686_Data_t imu_data_latest;

/* Pointers to MCU domain hardware (Dependency Injection) */
static SPI_TypeDef *imu_spi = NULL;
static BDMA_Channel_TypeDef *imu_bdma_tx = NULL;
static BDMA_Channel_TypeDef *imu_bdma_rx = NULL;
static void (*imu_rx_complete_cb)(ICM45686_Data_t *data) = NULL;

/* --- PRIVATE HELPER FUNCTIONS --- */

static inline uint8_t ICM45686_ReadRegister(uint8_t reg)
{
    uint8_t tx_buf[2] = { (uint8_t)(reg | ICM45686_SPI_READ_BIT), 0x00 };
    uint8_t rx_buf[2] = { 0 };

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(imu_spi, tx_buf, rx_buf, 2);
    GPIO_SET(IMU2_CS);

    return rx_buf[1];
}

/* --- PUBLIC API IMPLEMENTATION --- */

void ICM45686_Init(const ICM45686_HwConfig_t *hw_config)
{
    /* 1. Store hardware configuration (Dependency Injection) */
    imu_spi = hw_config->SPIx;
    imu_bdma_tx = hw_config->BDMA_Tx;
    imu_bdma_rx = hw_config->BDMA_Rx;
    imu_rx_complete_cb = hw_config->RxCompleteCb;

    /* 2. Software Reset */
    /* WHO_AM_I was unstable without reset (did not always respond correctly) */
    ICM45686_WriteRegister(ICM45686_REG_MISC2, 0x02); // e.g., 0x01 for soft reset
    Delay_ms(10); /* Time for IMU registers to reload after soft reset */

    /* 3. Hardware Sanity Check (WHO_AM_I) - IMU-soft-reset */
    uint8_t who_am_i = ICM45686_ReadRegister(ICM45686_REG_WHO_AM_I);
    
    if (who_am_i != ICM45686_WHO_AM_I_VAL)
    {
        /* Fatal error: Sensor not detected or SPI failure. Trap execution. */
        while (1) { }
    }

    /* 4. Configure IMU Internal Registers for DRDY Interrupt */
    ICM45686_Config();
}

void ICM45686_WriteRegister(uint8_t reg, uint8_t value)
{
    /* Clear bit 7 to ensure a SPI write operation */
    uint8_t tx_buf[2] = { (uint8_t)(reg & ~ICM45686_SPI_READ_BIT), value };

    GPIO_RESET(IMU2_CS);
    SPI_Transmit_Blocking(imu_spi, tx_buf, 2);
    GPIO_SET(IMU2_CS);
}

void ICM45686_Config(void)
{
    ICM45686_WriteRegister(ICM45686_REG_INTF_CONFIG1_OVRD, ICM45686_INTF_CONFIG1_OVRD_4WIRE);
    ICM45686_WriteRegister(ICM45686_REG_INT1_CONFIG2, ICM45686_INT1_CONFIG2_PP_PULSE_AH);
    ICM45686_WriteRegister(ICM45686_REG_INT1_CONFIG0, ICM45686_INT1_CONFIG0_DRDY_EN);
    ICM45686_WriteRegister(ICM45686_REG_ACCEL_CONFIG0, ICM45686_ACCEL_CONFIG0_8G_200HZ);
    ICM45686_WriteRegister(ICM45686_REG_GYRO_CONFIG0, ICM45686_GYRO_CONFIG0_2000DPS_200HZ);
    ICM45686_WriteRegister(ICM45686_REG_PWR_MGMT0, ICM45686_PWR_MGMT0_ACCEL_GYRO_LN);

    Delay_ms(200);
}

bool ICM45686_IsDataReady(void)
{
    uint8_t tx_buf[2] = { ICM45686_REG_INT1_STATUS0 | ICM45686_SPI_READ_BIT, 0x00 };
    uint8_t rx_buf[2] = { 0x00, 0x00 };

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(imu_spi, tx_buf, rx_buf, 2);
    GPIO_SET(IMU2_CS);

    return (rx_buf[1] & ICM45686_INT1_STATUS0_DRDY_MASK) != 0U;
}

void ICM45686_ReadDataBurst(ICM45686_Data_t *data)
{
    uint8_t tx_buf[15] = {0};
    uint8_t rx_buf[15] = {0};

    tx_buf[0] = ICM45686_REG_ACCEL_DATA_X0_UI | ICM45686_SPI_READ_BIT;

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(imu_spi, tx_buf, rx_buf, 15);
    GPIO_SET(IMU2_CS);

    data->accel[0] = (int16_t)((rx_buf[2]  << 8) | rx_buf[1]);
    data->accel[1] = (int16_t)((rx_buf[4]  << 8) | rx_buf[3]);
    data->accel[2] = (int16_t)((rx_buf[6]  << 8) | rx_buf[5]);
    data->gyro[0]  = (int16_t)((rx_buf[8]  << 8) | rx_buf[7]);
    data->gyro[1]  = (int16_t)((rx_buf[10] << 8) | rx_buf[9]);
    data->gyro[2]  = (int16_t)((rx_buf[12] << 8) | rx_buf[11]);
    data->temp     = (int16_t)((rx_buf[14] << 8) | rx_buf[13]);
}

void ICM45686_StartDMAReadBurst(void)
{
    imu_tx_buf[0] = ICM45686_REG_ACCEL_DATA_X0_UI | ICM45686_SPI_READ_BIT;
    
    SCB_CleanDCache_by_Addr((uint32_t*)imu_tx_buf, 32);
    
    GPIO_RESET(IMU2_CS);

    SPI_TransmitReceive_DMA(imu_spi, imu_bdma_tx, imu_bdma_rx, imu_tx_buf, imu_rx_buf, 15);
}

void ICM45686_DMA_RxComplete_Callback(void)
{
    GPIO_SET(IMU2_CS);

    imu_spi->CR1 &= ~SPI_CR1_SPE;
    imu_spi->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);

    SCB_InvalidateDCache_by_Addr((uint32_t*)imu_rx_buf, 32);

    imu_data_latest.accel[0] = (int16_t)((imu_rx_buf[2]  << 8) | imu_rx_buf[1]);
    imu_data_latest.accel[1] = (int16_t)((imu_rx_buf[4]  << 8) | imu_rx_buf[3]);
    imu_data_latest.accel[2] = (int16_t)((imu_rx_buf[6]  << 8) | imu_rx_buf[5]);
    imu_data_latest.gyro[0]  = (int16_t)((imu_rx_buf[8]  << 8) | imu_rx_buf[7]);
    imu_data_latest.gyro[1]  = (int16_t)((imu_rx_buf[10] << 8) | imu_rx_buf[9]);
    imu_data_latest.gyro[2]  = (int16_t)((imu_rx_buf[12] << 8) | imu_rx_buf[11]);
    imu_data_latest.temp     = (int16_t)((imu_rx_buf[14] << 8) | imu_rx_buf[13]);

    /* Call callback instead of direct coupling with Flight Loop */
    if (imu_rx_complete_cb != NULL)
    {
        imu_rx_complete_cb(&imu_data_latest);
    }
}