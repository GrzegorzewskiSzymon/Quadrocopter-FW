/*
 * drivers_config.c
 *
 *  Created on: May 14, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "stm32h723xx.h"
#include "drivers_config.h"
#include "board_config.h"
#include "gpio.h"
#include "spi.h"
#include "nrf24l01.h"
#include "icm45686.h"
#include "control_loop.h"
#include "led.h"

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


void BOARD_IMU_Init(void)
{
    /* 1. Clocks (For now, eventually in BOARD_Clocks_Init) */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    RCC->APB4ENR |= RCC_APB4ENR_SPI6EN;
    (void)RCC->APB4ENR; 

    /* 2. SPI6 Pins (ICM45686) */
    GPIO_INIT(IMU2_CS, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_SET(IMU2_CS);
    GPIO_INIT(IMU2_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(IMU2_SCK, 8U);
    GPIO_INIT(IMU2_MISO, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_PU);
    GPIO_INIT_AF(IMU2_MISO, 5U);
    GPIO_INIT(IMU2_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(IMU2_MOSI, 8U);

    /* 3. SPI6 Hardware Configuration */
    SPI_Config_t spi6_cfg = {
        .Mode      = SPI_MODE_MASTER,
        .Direction = SPI_DIR_FULL_DUPLEX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos), /* MBR=4 -> prescaler /32 */
        .DataSize  = 8,
        .CPOL      = true,
        .CPHA      = true
    };
    SPI_Init(SPI6, &spi6_cfg);

    /* 4. Interrupt Pin Configuration (PA4) and EXTI4 */
    GPIO_INIT(IMU2_INT, GPIO_MODE_INPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI4_Msk;
    SYSCFG->EXTICR[1] |= (0x00U << SYSCFG_EXTICR2_EXTI4_Pos); /* Port A */
    EXTI->RTSR1 |= EXTI_RTSR1_TR4;
    EXTI->FTSR1 &= ~EXTI_FTSR1_TR4;
    EXTI->IMR1 |= EXTI_IMR1_IM4;
    NVIC_SetPriority(EXTI4_IRQn, 1);
    NVIC_EnableIRQ(EXTI4_IRQn);

    /* 5. Inject hardware configuration into IMU driver */
    ICM45686_HwConfig_t imu_hw = {
        .SPIx = SPI6,
        .BDMA_Tx = BDMA_Channel1,
        .BDMA_Rx = BDMA_Channel0,
        .RxCompleteCb = ControlLoop_Execute /* Control Loop connected as Callback */
    };
    ICM45686_Init(&imu_hw);
}

/* Bridge triggering hardware DMA transfer (hides DMA flags from led.c file) */
static void BOARD_WS2812_Transmit(uint8_t *tx_buffer, uint32_t size)
{
    /* Clear interrupt flags for DMA1 stream 1 (TC & TE) */
    DMA1->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CTEIF1;
    
    SPI_Transmit_DMA(SPI4, DMA1_Stream1, tx_buffer, size);
}

void BOARD_LED_Init(void)
{
    /* 1. Discrete LEDs on board (Power/error signaling) */
    GPIO_INIT(LED_RED,    GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_YELLOW, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_GREEN,  GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_BLUE,   GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);

    GPIO_RESET(LED_RED);
    GPIO_RESET(LED_YELLOW);
    GPIO_RESET(LED_GREEN);
    GPIO_RESET(LED_BLUE);

    /* 2. Addressable LED pins (SPI4) */
    GPIO_INIT(LED_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_PD);
    GPIO_INIT_AF(LED_MOSI, 5U);
    GPIO_INIT(LED_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(LED_SCK, 5U);

    /* 3. Hardware configuration SPI4 */
    SPI_Config_t led_spi_cfg = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIR_SIMPLEX_TX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos),
        .DataSize = 8U,
        .CPOL = false,
        .CPHA = true 
    };
    SPI_Init(SPI4, &led_spi_cfg);

    /* 4. Injection of transmission bridge to LED driver */
    LED_HwConfig_t led_hw = {
        .TransmitCb = BOARD_WS2812_Transmit
    };
    LED_Init(&led_hw);
}

/* ========================================================================= */
/* ALL INTERRUPT SERVICE ROUTINES (ISR)                                          */
/* ========================================================================= */

/**
 * @trigger BDMA Channel 0 (D3 Domain) triggered by SPI6 RXNE event.
 * @flow   Delegates raw payload processing to ICM45686 driver -> triggers Flight Loop.
 */

void BDMA_CH0_IRQHandler(void)
{
    /* Standard transfer completion handling */
    if (BDMA->ISR & BDMA_ISR_TCIF0)
    {
        BDMA->IFCR = BDMA_IFCR_CGIF0 | BDMA_IFCR_CTCIF0;
        ICM45686_DMA_RxComplete_Callback();
    }
}

/**
 * @trigger DMA1 Stream2 (D2 Domain) (SPI3 RXNE event) transfer complete (NRF24 TX payload sent).
 * @flow   Delegates to NRF24 driver to finalize transaction
 */

void DMA_STR2_IRQHandler(void)
{
    /* Transfer Complete Interrupt for SPI3 RX */
    if (DMA1->LISR & DMA_LISR_TCIF2)
    {
        DMA1->LIFCR = DMA_LIFCR_CTCIF2; /* Clear flag */
        
        NRF24_DMA_RxComplete_Callback();
    }
}

/**
 * @brief  NRF24L01+ Hardware Interrupt handler.
 * @trigger Falling edge on RF_IRQ pin (mapped to EXTI Line 1).
 * @flow   Triggers NRF24 EXTI state machine to check status (TX_DS / MAX_RT) and flush buffers.
 */
void EXTI1_IRQHandler(void)
{
    /* Check and clear hardware interrupt flag from line 1 */
    if (EXTI->PR1 & EXTI_PR1_PR1)
    {
        EXTI->PR1 = EXTI_PR1_PR1; 

        NRF24_EXTI_Callback();
    }
}

/**
 * @trigger Rising edge on IMU2_INT pin (mapped to EXTI Line 4).
 * @flow   Initiates non-blocking BDMA background transaction to fetch IMU registers.
 * @critical Must be executed with minimal latency to ensure control loop determinism.
 */
 
void EXTI4_IRQHandler(void)
{
    /* Quick flag check and clear (Zero-Overhead) */
    if (EXTI->PR1 & EXTI_PR1_PR4)
    {
        EXTI->PR1 = EXTI_PR1_PR4; /* rc_w1 clears the flag */
        
        /* Start non-blocking DMA background transaction */
        ICM45686_StartDMAReadBurst();
    }
}