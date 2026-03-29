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

void ICM45686_Init(void)
{
    /* --- 1. GPIO configuration --- */

    GPIO_INIT(IMU2_CS, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_SET(IMU2_CS); /* Stan spoczynkowy (High) */

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
}


uint8_t ICM45686_ReadWhoAmI(void)
{
    uint8_t tx_buf[2] = { ICM45686_REG_WHO_AM_I | ICM45686_SPI_READ_BIT, 0x00 };
    uint8_t rx_buf[2] = { 0x00, 0x00 };

    GPIO_RESET(IMU2_CS);
    SPI_TransmitReceive_Blocking(SPI6, tx_buf, rx_buf, 2);
    GPIO_SET(IMU2_CS);

    return rx_buf[1];
}
