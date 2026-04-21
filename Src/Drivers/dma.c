/*
 * dma.c
 *
 * Created on: Apr 21, 2026
 * Author: Szymon Grzegorzewski
 */

#include "dma.h"
#include "icm45686.h"

void DMA_Init(void)
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