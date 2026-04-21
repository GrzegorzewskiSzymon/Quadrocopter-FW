/*
 * icm45686.c
 *
 *  Created on: Mar 29, 2026
 *      Author: Szymon Grzegorzewski
 */


#include "icm45686.h"
#include "gpio.h"
#include "spi.h"
#include "board_config.h"
#include "systick.h"
#include "dma.h"
#include "../Flight/control_loop.h"

/* CRITICAL: 32-byte alignment for D-Cache line boundary. Must be placed in D3 Domain (SRAM4) */
__attribute__((aligned(32), section(".sram4"))) static uint8_t imu_tx_buf[32];
__attribute__((aligned(32), section(".sram4"))) static uint8_t imu_rx_buf[32];

static inline uint8_t ICM45686_ReadRegister(uint8_t reg)
{
    uint8_t tx_buf[2] = { (uint8_t)(reg | ICM45686_SPI_READ_BIT), 0x00 };
    uint8_t rx_buf[2] = { 0 };

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(SPI6, tx_buf, rx_buf, 2);
    GPIO_SET(IMU2_CS);

    return rx_buf[1];
}


void ICM45686_Init(void)
{
    /* --- 1. GPIO configuration --- */
    GPIO_INIT(IMU2_CS, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_SET(IMU2_CS); /* Standby state (High) */

    GPIO_INIT(IMU2_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(IMU2_SCK, 8U);

    GPIO_INIT(IMU2_MISO, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_PU);
    GPIO_INIT_AF(IMU2_MISO, 5U);

    GPIO_INIT(IMU2_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(IMU2_MOSI, 8U);

    GPIO_SET(IMU2_CS);

    /* --- 2. Configuration of SPI6 --- */
    /* pclk4 = 275 MHz. Prescaler /32 (MBR=4) equals ~8.5 MHz. */
    SPI_Config_t spi_cfg = {
        .Mode      = SPI_MODE_MASTER,
        .Direction = SPI_DIR_FULL_DUPLEX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos),
        .DataSize  = 8,
        .CPOL      = true,
        .CPHA      = true
    };

    SPI_Init(SPI6, &spi_cfg);

    /* --- 3. Hardware Sanity Check (WHO_AM_I) --- */
    /* Wait 2ms for IMU to boot up after power-on sequence */
    Delay_ms(2);
    uint8_t who_am_i = ICM45686_ReadRegister(ICM45686_REG_WHO_AM_I);
    if (who_am_i != ICM45686_WHO_AM_I_VAL)
    {
        /* Fatal error: Sensor not detected or SPI failure. Trap execution. */
        while (1) { }
    }

    /* --- 3.5. Configure IMU Internal Registers for DRDY Interrupt --- */
    ICM45686_Config();

    /* --- 4. EXTI Configuration for IMU2_INT (PA4) --- */
    GPIO_INIT(IMU2_INT, GPIO_MODE_INPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);

    /* Enable SYSCFG clock for EXTI routing */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR; /* Delay for clock stabilization */

    /* Route PA4 to EXTI4. Value 0x00 corresponds to Port A. */
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI4_Msk;
    SYSCFG->EXTICR[1] |= (0x00U << SYSCFG_EXTICR2_EXTI4_Pos);

    /* Configure EXTI4 trigger to Rising Edge */
    EXTI->RTSR1 |= EXTI_RTSR1_TR4;
    EXTI->FTSR1 &= ~EXTI_FTSR1_TR4;

    /* Unmask EXTI4 interrupt */
    EXTI->IMR1 |= EXTI_IMR1_IM4;

    /* Configure NVIC for EXTI4. Hard Real-Time priority (e.g., 1). */
    NVIC_SetPriority(EXTI4_IRQn, 1);
    NVIC_EnableIRQ(EXTI4_IRQn);
}


void ICM45686_WriteRegister(uint8_t reg, uint8_t value)
{
    /* Clear bit 7 to ensure a SPI write operation */
    uint8_t tx_buf[2] = { (uint8_t)(reg & ~ICM45686_SPI_READ_BIT), value };

    GPIO_RESET(IMU2_CS);
    SPI_Transmit_Blocking(SPI6, tx_buf, 2);
    GPIO_SET(IMU2_CS);
}

/**
 * @brief Configures ICM-45686 internal registers.
 *
 */
void ICM45686_Config(void)
{
    /* Force 4-wire SPI mode via INTF_CONFIG1_OVRD override register. */
    ICM45686_WriteRegister(ICM45686_REG_INTF_CONFIG1_OVRD,
                           ICM45686_INTF_CONFIG1_OVRD_4WIRE);

    /* Configure INT1: push-pull, pulse mode, active-high (for rising edge EXTI). */
    ICM45686_WriteRegister(ICM45686_REG_INT1_CONFIG2,
                           ICM45686_INT1_CONFIG2_PP_PULSE_AH);

    /* Enable Data Ready flag in INT1_STATUS0. */
    ICM45686_WriteRegister(ICM45686_REG_INT1_CONFIG0,
                           ICM45686_INT1_CONFIG0_DRDY_EN);

    /* Configure accelerometer: +/-8g, 200 Hz. */
    ICM45686_WriteRegister(ICM45686_REG_ACCEL_CONFIG0,
                           ICM45686_ACCEL_CONFIG0_8G_200HZ);

    /* Configure gyroscope: +/-2000 dps, 200 Hz. */
    ICM45686_WriteRegister(ICM45686_REG_GYRO_CONFIG0,
                           ICM45686_GYRO_CONFIG0_2000DPS_200HZ);

    /* Activate sensors (always last). Wait ~200 ms for gyroscope readiness. */
    ICM45686_WriteRegister(ICM45686_REG_PWR_MGMT0,
                           ICM45686_PWR_MGMT0_ACCEL_GYRO_LN);

    Delay_ms(200);
}


/**
 * @brief Checks if new measurement data is ready (polling INT1_STATUS0).
 * @return true if data ready, false otherwise
 */
bool ICM45686_IsDataReady(void)
{
    uint8_t tx_buf[2] = { ICM45686_REG_INT1_STATUS0 | ICM45686_SPI_READ_BIT, 0x00 };
    uint8_t rx_buf[2] = { 0x00, 0x00 };

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(SPI6, tx_buf, rx_buf, 2);
    GPIO_SET(IMU2_CS);

    return (rx_buf[1] & ICM45686_INT1_STATUS0_DRDY_MASK) != 0U;
}

/**
 * @brief Reads a 14-byte data packet (Accel + Gyro + Temp) using Burst Read
 * @param data Pointer to the target structure
 */
void ICM45686_ReadDataBurst(ICM45686_Data_t *data)
{
    /* SPI burst read: 1 address + 14 data bytes. */
    uint8_t tx_buf[15] = {0};
    uint8_t rx_buf[15] = {0};

    /* Start from the first data register: Accel X (LSB) */
    tx_buf[0] = ICM45686_REG_ACCEL_DATA_X0_UI | ICM45686_SPI_READ_BIT;

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(SPI6, tx_buf, rx_buf, 15);
    GPIO_SET(IMU2_CS);

    /* * Parse Little-Endian. 
     * Bit shifts guarantee no unaligned access errors, 
     * and the compiler (GCC ARM) will optimize this to native LDRH instructions.
     */
    data->accel[0] = (int16_t)((rx_buf[2]  << 8) | rx_buf[1]);  /* Accel X */
    data->accel[1] = (int16_t)((rx_buf[4]  << 8) | rx_buf[3]);  /* Accel Y */
    data->accel[2] = (int16_t)((rx_buf[6]  << 8) | rx_buf[5]);  /* Accel Z */
    
    data->gyro[0]  = (int16_t)((rx_buf[8]  << 8) | rx_buf[7]);  /* Gyro X */
    data->gyro[1]  = (int16_t)((rx_buf[10] << 8) | rx_buf[9]);  /* Gyro Y */
    data->gyro[2]  = (int16_t)((rx_buf[12] << 8) | rx_buf[11]); /* Gyro Z */
    
    data->temp     = (int16_t)((rx_buf[14] << 8) | rx_buf[13]); /* Temp */
}




static ICM45686_Data_t imu_data_latest;

void ICM45686_StartDMAReadBurst(void)
{
    /* Prepare the register address for burst read */
    imu_tx_buf[0] = ICM45686_REG_ACCEL_DATA_X0_UI | ICM45686_SPI_READ_BIT;
    
    /* Clean D-Cache: Push CPU changes from Cache to Main Memory (SRAM4) so BDMA sees it */
    SCB_CleanDCache_by_Addr((uint32_t*)imu_tx_buf, 32);
    
    /* Assert CS */
    GPIO_RESET(IMU2_CS);

    /* Start DMA Transaction: 1 Byte Register + 14 Bytes Payload = 15 Bytes */
    SPI_TransmitReceive_DMA(SPI6, BDMA_Channel1, BDMA_Channel0, imu_tx_buf, imu_rx_buf, 15);
}

void ICM45686_DMA_RxComplete_Callback(void)
{
    /* De-assert CS immediately upon DMA RX completion */
    GPIO_SET(IMU2_CS);

    /* Optional: Disable DMA requests on SPI6 to maintain clean state */
    SPI6->CR1 &= ~SPI_CR1_SPE;
    SPI6->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);

    /* Invalidate D-Cache: BDMA updated Main Memory. CPU must fetch fresh data, not old Cache */
    SCB_InvalidateDCache_by_Addr((uint32_t*)imu_rx_buf, 32);

    /* Parse data (Little-Endian) */
    imu_data_latest.accel[0] = (int16_t)((imu_rx_buf[2]  << 8) | imu_rx_buf[1]);
    imu_data_latest.accel[1] = (int16_t)((imu_rx_buf[4]  << 8) | imu_rx_buf[3]);
    imu_data_latest.accel[2] = (int16_t)((imu_rx_buf[6]  << 8) | imu_rx_buf[5]);
    
    imu_data_latest.gyro[0]  = (int16_t)((imu_rx_buf[8]  << 8) | imu_rx_buf[7]);
    imu_data_latest.gyro[1]  = (int16_t)((imu_rx_buf[10] << 8) | imu_rx_buf[9]);
    imu_data_latest.gyro[2]  = (int16_t)((imu_rx_buf[12] << 8) | imu_rx_buf[11]);
    
    imu_data_latest.temp     = (int16_t)((imu_rx_buf[14] << 8) | imu_rx_buf[13]);

    /* Execute Hard Real-Time Control Loop (Pass pointer to avoid copy overhead) */
    ControlLoop_Execute(&imu_data_latest);
}