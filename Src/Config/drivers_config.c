/*
 * drivers_config.c
 *
 *  Created on: May 14, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "stm32h723xx.h"
#include "board_config.h"
#include "gpio.h"
#include "spi.h"
#include "nrf24l01.h"

void BOARD_Radio_Init(void)
{
    /* 1. Clocks */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    RCC->APB1LENR |= RCC_APB1LENR_SPI3EN;
    (void)RCC->AHB4ENR; /* DSB/ISB alternative */

    /* 2. SPI3 Pins */
    GPIO_INIT(RF_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_SCK, 6U);
    GPIO_INIT(RF_MISO, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_MISO, 6U);
    GPIO_INIT(RF_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_MOSI, 6U);

    /* 3. RF Control Pins */
    GPIO_INIT(RF_CSN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_SET(RF_CSN);
    GPIO_INIT(RF_CE, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_CE);
    GPIO_INIT(RF_TXEN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_TXEN);
    GPIO_INIT(RF_RXEN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_RXEN);
    GPIO_INIT(RF_IRQ, GPIO_MODE_INPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PU);

    /* 4. SPI3 Hardware Configuration */
    SPI_Config_t spi3_cfg = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIR_FULL_DUPLEX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos), 
        .DataSize = 8,
        .CPOL = false,
        .CPHA = false
    };
    SPI_Init(SPI3, &spi3_cfg);

    /* 5. EXTI Interrupt from NRF */
    extern void GPIO_NRF_EXTI_Init(void); // Temporarily, eventually EXTI will also be moved to board_init
    GPIO_NRF_EXTI_Init();

    /* 6. Inject hardware configuration into driver */
    NRF24_HwConfig_t nrf_hw = {
        .SPIx = SPI3,
        .DMA_Tx = DMA1_Stream3,
        .DMA_Rx = DMA1_Stream2
    };
    NRF24_Init(&nrf_hw);
}