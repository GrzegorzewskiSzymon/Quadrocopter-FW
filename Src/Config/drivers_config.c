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
#include "rcc.h"

Status_t BOARD_Radio_Init(void)
{
    /* 1. Clocks */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    (void)RCC->AHB4ENR; /* DSB/ISB alternative */
    RCC_EnableSPIClock(SPI3);

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
    GPIO_SET(RF_CE); //TODO: Implement CE control based on state machine to save power.
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
        .CPHA = false,
        .HoldPinsWhenDisabled = true /* SPE is toggled while CSN is low */
    };
    SPI_Init(SPI3, &spi3_cfg);

    /* 5. Inject hardware configuration into driver */
    NRF24_HwConfig_t nrf_hw = {
        .SPIx = SPI3,
        .DMA_Tx = DMA1_Stream3,
        .DMA_Rx = DMA1_Stream2
    };
    Status_t status = NRF24_Init(&nrf_hw);
    if (status != STATUS_OK)
    {
        /* IRQ stays disabled: the TX chain must not run on a radio that failed setup */
        return status;
    }

    /* 6. EXTI from NRF only after the driver holds its SPI handle: an IRQ glitch
          during power-up would otherwise run the callback on a NULL peripheral */
    extern void GPIO_NRF_EXTI_Init(void); // Temporarily, eventually EXTI will also be moved to board_init
    GPIO_NRF_EXTI_Init();

    return STATUS_OK;
}


/* SPI6 is dedicated to the IMU: its BDMA channels and the stall recovery below
   reset the whole peripheral, which would break any other device on this bus */
static const SPI_Config_t imu_spi_cfg = {
    .Mode      = SPI_MODE_MASTER,
    .Direction = SPI_DIR_FULL_DUPLEX,
    .Prescaler = (4U << SPI_CFG1_MBR_Pos), /* MBR=4 -> prescaler /32 */
    .DataSize  = 8,
    .CPOL      = true,
    .CPHA      = true,
    .HoldPinsWhenDisabled = true /* SPE is toggled while CS is low */
};

/* Called with IRQs masked on a stalled burst: clock enable + RCC reset + config bring
   SPI6 back from any internal state; DMAEN is restored by the next BDMA transfer */
static void BOARD_IMU_BusRecover(void)
{
    RCC_EnableSPIClock(SPI6);
    SPI_Init(SPI6, &imu_spi_cfg);
}

Status_t BOARD_IMU_Init(void)
{
    /* 1. Clocks */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR; 
    RCC_EnableSPIClock(SPI6);

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

    SPI_Init(SPI6, &imu_spi_cfg);

    /* 4. Interrupt Pin Configuration (PA4) and EXTI4 routing; line stays masked for now */
    GPIO_INIT(IMU2_INT, GPIO_MODE_INPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    SYSCFG->EXTICR[1] &= ~SYSCFG_EXTICR2_EXTI4_Msk;
    SYSCFG->EXTICR[1] |= (0x00U << SYSCFG_EXTICR2_EXTI4_Pos); /* Port A */
    EXTI->RTSR1 |= EXTI_RTSR1_TR4;
    EXTI->FTSR1 &= ~EXTI_FTSR1_TR4;

    /* 5. Inject hardware configuration into IMU driver */
    ICM45686_HwConfig_t imu_hw = {
        .SPIx = SPI6,
        .BDMA_Tx = BDMA_Channel1,
        .BDMA_Rx = BDMA_Channel0,
        .RxCompleteCb = ControlLoop_Execute, /* Control Loop connected as Callback */
        .BusRecoverCb = BOARD_IMU_BusRecover
    };
    Status_t status = ICM45686_Init(&imu_hw);
    if (status != STATUS_OK)
    {
        /* DRDY stays masked: no DMA burst may run against an absent or misconfigured sensor */
        return status;
    }

    /* 6. Unmask DRDY only now: init uses blocking SPI6 transfers that a DMA burst
          started from EXTI4 would collide with. Drop edges latched during init. */
    EXTI->PR1 = EXTI_PR1_PR4;
    NVIC_ClearPendingIRQ(EXTI4_IRQn);
    EXTI->IMR1 |= EXTI_IMR1_IM4;
    NVIC_SetPriority(EXTI4_IRQn, 1);
    NVIC_EnableIRQ(EXTI4_IRQn);

    return STATUS_OK;
}

/* Bridge triggering hardware DMA transfer (hides DMA flags from led.c file) */
static void BOARD_WS2812_Transmit(uint8_t *tx_buffer, uint32_t size)
{
    /* Clear interrupt flags for DMA1 stream 1 (TC & TE) */
    DMA1->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CTEIF1;
    
    /* LED frames are best effort and refreshed every 10 ms; a stream that fails to stop
       only skips this frame */
    (void)SPI_Transmit_DMA(SPI4, DMA1_Stream1, tx_buffer, size);
}

void BOARD_LED_Init(void)
{
    /* 1. Clocks */
    RCC_EnableSPIClock(SPI4);

    /* 2. Discrete LEDs on board (Power/error signaling) */
    GPIO_INIT(LED_RED,    GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_YELLOW, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_GREEN,  GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
    GPIO_INIT(LED_BLUE,   GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);

    GPIO_RESET(LED_RED);
    GPIO_RESET(LED_YELLOW);
    GPIO_RESET(LED_GREEN);
    GPIO_RESET(LED_BLUE);

    /* 3. Addressable LED pins (SPI4) */
    GPIO_INIT(LED_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_PD);
    GPIO_INIT_AF(LED_MOSI, 5U);
    GPIO_INIT(LED_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(LED_SCK, 5U);

    /* 4. Hardware configuration SPI4 */
    SPI_Config_t led_spi_cfg = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIR_SIMPLEX_TX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos),
        .DataSize = 8U,
        .CPOL = false,
        .CPHA = true,
        .HoldPinsWhenDisabled = false /* WS2812 needs MOSI idle low; level held by AFCNTR not verified */
    };
    SPI_Init(SPI4, &led_spi_cfg);

    /* 5. Injection of transmission bridge to LED driver */
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
    uint32_t isr = BDMA->ISR;

    /* CGIF0 clears every channel 0 flag (GIF, TCIF, HTIF, TEIF) */
    BDMA->IFCR = BDMA_IFCR_CGIF0;

    /* Error takes precedence: on TE the received frame is incomplete */
    if (isr & BDMA_ISR_TEIF0)
    {
        ICM45686_DMA_Error_Callback();
    }
    else if (isr & BDMA_ISR_TCIF0)
    {
        ICM45686_DMA_RxComplete_Callback();
    }
}

/**
 * @trigger BDMA Channel 1 (SPI6 TX) transfer error. TC is not enabled on this channel.
 */
void BDMA_CH1_IRQHandler(void)
{
    uint32_t isr = BDMA->ISR;

    BDMA->IFCR = BDMA_IFCR_CGIF1;

    if (isr & BDMA_ISR_TEIF1)
    {
        ICM45686_DMA_Error_Callback();
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
        /* Clear ALL flags of RX (Stream2) and TX (Stream3). Hardware refuses to set EN
           on a stream with stale flags, so a leftover TCIF3 would block the next TX. */
        DMA1->LIFCR = DMA_LIFCR_CTCIF2 | DMA_LIFCR_CHTIF2 | DMA_LIFCR_CTEIF2 | DMA_LIFCR_CDMEIF2 | DMA_LIFCR_CFEIF2
                    | DMA_LIFCR_CTCIF3 | DMA_LIFCR_CHTIF3 | DMA_LIFCR_CTEIF3 | DMA_LIFCR_CDMEIF3 | DMA_LIFCR_CFEIF3;

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