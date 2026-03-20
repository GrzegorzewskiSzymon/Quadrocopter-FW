/*
 * spi.c
 *
 *  Created on: Mar 20, 2026
 *      Author: Szymon Grzegorzewski
 */
#include "spi.h"

void SPI_Init(SPI_TypeDef *SPIx, const SPI_Config_t *config)
{
    /* Temporary solution for SPI4 TODO: will eventually be handled in rcc.c */
    if (SPIx == SPI4)
    {
        RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
        (void)RCC->APB2ENR;
        RCC->APB2RSTR |= RCC_APB2RSTR_SPI4RST;
        (void)RCC->APB2RSTR; 
        RCC->APB2RSTR &= ~RCC_APB2RSTR_SPI4RST;
        (void)RCC->APB2RSTR; 
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
        while ((SPIx->SR & SPI_SR_TXP) == 0) { }
        *((__IO uint8_t *)&SPIx->TXDR) = data[i];
    }

    while ((SPIx->SR & SPI_SR_EOT) == 0) { }
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
}
