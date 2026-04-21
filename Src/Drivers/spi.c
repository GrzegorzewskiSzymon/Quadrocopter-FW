/*
 * spi.c
 *
 *  Created on: Mar 20, 2026
 *      Author: Szymon Grzegorzewski
 */
#include "spi.h"

void SPI_Init(SPI_TypeDef *SPIx, const SPI_Config_t *config)
{
    /* Temporary solution for SPI4 & SPI6 TODO: RCC driver */
    if (SPIx == SPI4) {
        RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
        (void)RCC->APB2ENR;
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI4RST;
        (void)RCC->APB2RSTR; 
        RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI4RST;
        (void)RCC->APB2RSTR; 
    }
    else if (SPIx == SPI6) {
        RCC->APB4ENR |= RCC_APB4ENR_SPI6EN;
        (void)RCC->APB4ENR;
        RCC->APB4RSTR |= RCC_APB4RSTR_SPI6RST;
        (void)RCC->APB4RSTR; 
        RCC->APB4RSTR &= ~RCC_APB4RSTR_SPI6RST;
        (void)RCC->APB4RSTR; 
    }

    SPIx->CR1 &= ~SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_SSI;

    uint32_t cfg1 = config->Prescaler 
                  | ((config->DataSize - 1U) << SPI_CFG1_DSIZE_Pos)
                  | (0U << SPI_CFG1_FTHLV_Pos);
    SPIx->CFG1 = cfg1;

    uint32_t cfg2 = SPI_CFG2_SSM;
    if (config->Mode == SPI_MODE_MASTER) cfg2 |= SPI_CFG2_MASTER;
    if (config->Direction == SPI_DIR_SIMPLEX_TX) cfg2 |= SPI_CFG2_COMM_0;
    if (config->CPHA) cfg2 |= SPI_CFG2_CPHA;
    if (config->CPOL) cfg2 |= SPI_CFG2_CPOL;

    SPIx->CFG2 = cfg2;
    SPIx->CR1 |= SPI_CR1_SPE;
}

void SPI_Transmit_Blocking(SPI_TypeDef *SPIx, const uint8_t *data, uint32_t size)
{
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_UDRC | 
                 SPI_IFCR_OVRC | SPI_IFCR_CRCEC | SPI_IFCR_MODFC | SPI_IFCR_TIFREC;

    SPIx->CR2 = size;
    SPIx->CR1 |= SPI_CR1_CSTART;

    for (uint32_t i = 0; i < size; i++)
    {
        /* Wait for free space in TX FIFO */
        while ((SPIx->SR & SPI_SR_TXP) == 0) { }
        *((__IO uint8_t *)&SPIx->TXDR) = data[i];

        /* CRITICAL: Wait for byte return and remove it from RX FIFO */
        while ((SPIx->SR & SPI_SR_RXP) == 0) { }
        volatile uint8_t dummy = *((__IO uint8_t *)&SPIx->RXDR);
        (void)dummy; /* Prevents compiler warnings */
    }

    while ((SPIx->SR & SPI_SR_EOT) == 0) { }
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
}

void SPI_TransmitReceive_Blocking(SPI_TypeDef *SPIx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size)
{
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_UDRC | 
                 SPI_IFCR_OVRC | SPI_IFCR_CRCEC | SPI_IFCR_MODFC | SPI_IFCR_TIFREC;

    SPIx->CR2 = size;
    SPIx->CR1 |= SPI_CR1_CSTART;

    for (uint32_t i = 0; i < size; i++)
    {
        while ((SPIx->SR & SPI_SR_TXP) == 0) { }
        *((__IO uint8_t *)&SPIx->TXDR) = tx_data[i];

        while ((SPIx->SR & SPI_SR_RXP) == 0) { }
        rx_data[i] = *((__IO uint8_t *)&SPIx->RXDR);
    }

    while ((SPIx->SR & SPI_SR_EOT) == 0) { }
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
}


void SPI_TransmitReceive_DMA(SPI_TypeDef *SPIx, BDMA_Channel_TypeDef *BDMA_Tx, BDMA_Channel_TypeDef *BDMA_Rx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size)
{
    /* SPI must be disabled before CFG1 register modification */
    SPIx->CR1 &= ~SPI_CR1_SPE;

    /* Absolute clearing of all status flags and errors */
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_UDRC | 
                 SPI_IFCR_OVRC | SPI_IFCR_CRCEC | SPI_IFCR_MODFC | SPI_IFCR_TIFREC;

    /* BDMA RX channel configuration */
    BDMA_Rx->CCR &= ~BDMA_CCR_EN;
    BDMA_Rx->CPAR = (uint32_t)&SPIx->RXDR;
    BDMA_Rx->CM0AR = (uint32_t)rx_data;  /* POPRAWKA: CM0AR zamiast CMAR */
    BDMA_Rx->CNDTR = size;
    BDMA_Rx->CCR |= BDMA_CCR_EN;

    /* BDMA TX channel configuration */
    BDMA_Tx->CCR &= ~BDMA_CCR_EN;
    BDMA_Tx->CPAR = (uint32_t)&SPIx->TXDR;
    BDMA_Tx->CM0AR = (uint32_t)tx_data;  /* POPRAWKA: CM0AR zamiast CMAR */
    BDMA_Tx->CNDTR = size;
    BDMA_Tx->CCR |= BDMA_CCR_EN;

    /* Connect DMA requests to SPI state machine */
    SPIx->CFG1 |= SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN; 

    /* STM32H7 requires explicit specification of transaction size in Master mode */
    SPIx->CR2 = size;

    /* Start hardware Master transaction */
    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;
}