/*
 * dma.c
 *
 * Created on: Apr 21, 2026
 * Author: Szymon Grzegorzewski
 */

#include "dma.h"
#include "icm45686.h"
#include "stm32h723xx.h"

void BDMA_Init(void)
{
    /* 1. Enable BDMA clock (Powers both BDMA and DMAMUX2 in D3 domain) */
    RCC->AHB4ENR |= RCC_AHB4ENR_BDMAEN;
    (void)RCC->AHB4ENR; /* DSB/ISB alternative for clock stabilization */

    /* 2. Configure DMAMUX2 for SPI6 */
    /* Request 11: SPI6_RX -> BDMA Channel 0 */
    /* Request 12: SPI6_TX -> BDMA Channel 1 */
    DMAMUX2_Channel0->CCR = 11U; 
    DMAMUX2_Channel1->CCR = 12U; 

    /* 3. Configure BDMA Channel 0 (RX) */
    BDMA_Channel0->CCR = BDMA_CCR_PL_1   /* Priority: High */
                       | BDMA_CCR_MINC   /* Memory increment mode */
                       | BDMA_CCR_TCIE;  /* Transfer Complete Interrupt Enable */
    /* DIR bit is 0 by default (Peripheral to Memory) */

    /* 4. Configure BDMA Channel 1 (TX) */
    BDMA_Channel1->CCR = BDMA_CCR_PL_1   /* Priority: High */
                       | BDMA_CCR_MINC   /* Memory increment mode */
                       | BDMA_CCR_DIR;   /* Memory to Peripheral */

    /* 5. Enable NVIC Interrupt for BDMA Rx Complete (Hard Real-Time Priority) */
    NVIC_SetPriority(BDMA_Channel0_IRQn, 1);
    NVIC_EnableIRQ(BDMA_Channel0_IRQn);
}


void DMA1_Init(void)
{
    /* 1. Enable DMA1 clock (Powers both DMA1 and DMAMUX1 in D2 domain) */
    RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
    (void)RCC->AHB1ENR; /* DSB/ISB alternative for clock stabilization */

    /* 2. Configure DMAMUX1 for SPI4 */
    /* Request 12: SPI4_TX -> DMA Channel 1 */
    DMAMUX1_Channel1->CCR = 84U; 

    /* 4. Configure DMA1 Channel 1 (TX) */
    DMA1_Stream1->CR =                    /* Priority: Low */
                        DMA_SxCR_MINC     /* Memory increment mode */
                      | DMA_SxCR_DIR_0;   /* Memory to Peripheral */


    /* SPI3_RX -> DMAMUX1 Request 61 -> DMA1 Stream 2 */
    DMAMUX1_Channel2->CCR = 61U; 
    DMA1_Stream2->CR = DMA_SxCR_MINC   /* Memory increment */
                     | DMA_SxCR_TCIE;  /* Enable Transfer Complete interrupt for RX! */
                     /* DIR = 0 by default (Peripheral to Memory) */

    /* SPI3_TX -> DMAMUX1 Request 62 -> DMA1 Stream 3 */
    DMAMUX1_Channel3->CCR = 62U; 
    DMA1_Stream3->CR = DMA_SxCR_MINC   /* Memory increment */
                     | DMA_SxCR_DIR_0; /* Memory to Peripheral */

    /* High priority RX interrupt (release CSN and CE pulse) */
    NVIC_SetPriority(DMA1_Stream2_IRQn, 2);
    NVIC_EnableIRQ(DMA1_Stream2_IRQn);
}


void BDMA_CH0_IRQHandler(void)
{
    /* 1. Check critical bus error (e.g., buffer outside D3 domain) */
    if (BDMA->ISR & BDMA_ISR_TEIF0)
    {
        BDMA->IFCR = BDMA_IFCR_CTEIF0;
        /* Debugger trap - if code enters here, buffers are NOT in SRAM4! */
        __asm volatile("bkpt #0"); 
    }

    /* 2. Standard transfer completion handling */
    if (BDMA->ISR & BDMA_ISR_TCIF0)
    {
        BDMA->IFCR = BDMA_IFCR_CGIF0 | BDMA_IFCR_CTCIF0;
        ICM45686_DMA_RxComplete_Callback();
    }
}

/* DMA1 Stream2 */
void DMA_STR2_IRQHandler(void)
{
    /* Transfer Complete Interrupt for SPI3 RX */
    if (DMA1->LISR & DMA_LISR_TCIF2)
    {
        DMA1->LIFCR = DMA_LIFCR_CTCIF2; /* Clear flag */
        
        /* Call hardware callback for NRF */
        extern void NRF24_DMA_RxComplete_Callback(void);
        NRF24_DMA_RxComplete_Callback();
    }
}