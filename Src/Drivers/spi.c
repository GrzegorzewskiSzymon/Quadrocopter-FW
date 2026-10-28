/*
 * spi.c
 *
 *  Created on: Mar 20, 2026
 *      Author: Szymon Grzegorzewski
 */
#include "spi.h"
#include "dwt.h"

/* Budget per awaited flag. One frame at the slowest prescaler (/256 of 137.5 MHz) takes ~15 us,
   so hitting this means the peripheral is stuck, not slow */
#define SPI_WAIT_TIMEOUT_US     1000UL

#define SPI_IFCR_ALL  (SPI_IFCR_EOTC | SPI_IFCR_TXTFC | SPI_IFCR_UDRC | SPI_IFCR_OVRC | \
                       SPI_IFCR_CRCEC | SPI_IFCR_MODFC | SPI_IFCR_TIFREC)

/* TSIZE may only be changed while the SPI is disabled (RM0468 SPI_CR2). Writing it with
   SPE set made a 2-frame session end after the first frame on HW, depending on code timing */
static inline void SPI_PrepareTransfer(SPI_TypeDef *SPIx, uint32_t size)
{
    SPIx->CR1 &= ~SPI_CR1_SPE;
    SPIx->IFCR = SPI_IFCR_ALL;
    SPIx->CR2 = size;
}

static Status_t SPI_WaitFlag(SPI_TypeDef *SPIx, uint32_t flag)
{
    uint32_t start = DWT_GetCycles();

    while ((SPIx->SR & flag) == 0U)
    {
        if (DWT_IsElapsedUs(start, SPI_WAIT_TIMEOUT_US))
        {
            /* Clearing SPE stops the master and flushes the FIFOs, so the next transfer starts clean */
            SPIx->CR1 &= ~SPI_CR1_SPE;
            SPIx->IFCR = SPI_IFCR_ALL;
            return STATUS_ERR_TIMEOUT;
        }
    }
    return STATUS_OK;
}

/* DMA1/DMA2 ignore register writes until the stream has really stopped (RM0468 DMA_SxCR.EN) */
static Status_t SPI_DisableDmaStream(DMA_Stream_TypeDef *stream)
{
    uint32_t start = DWT_GetCycles();

    stream->CR &= ~DMA_SxCR_EN;
    while ((stream->CR & DMA_SxCR_EN) != 0U)
    {
        if (DWT_IsElapsedUs(start, SPI_WAIT_TIMEOUT_US))
        {
            return STATUS_ERR_TIMEOUT;
        }
    }
    return STATUS_OK;
}

void SPI_Init(SPI_TypeDef *SPIx, const SPI_Config_t *config)
{
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
    /* Must never be set in slave mode (RM0468 SPI_CFG2.AFCNTR) */
    if (config->HoldPinsWhenDisabled && (config->Mode == SPI_MODE_MASTER)) cfg2 |= SPI_CFG2_AFCNTR;

    SPIx->CFG2 = cfg2;
    SPIx->CR1 |= SPI_CR1_SPE;
}

Status_t SPI_Transmit_Blocking(SPI_TypeDef *SPIx, const uint8_t *data, uint32_t size)
{
    SPI_PrepareTransfer(SPIx, size);
    /* Requests left from a DMA session would let a DMA stream steal FIFO data */
    SPIx->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;

    for (uint32_t i = 0; i < size; i++)
    {
        if (SPI_WaitFlag(SPIx, SPI_SR_TXP) != STATUS_OK) return STATUS_ERR_TIMEOUT;
        *((__IO uint8_t *)&SPIx->TXDR) = data[i];

        /* Full duplex: every sent frame returns one that must be drained from the RX FIFO */
        if (SPI_WaitFlag(SPIx, SPI_SR_RXP) != STATUS_OK) return STATUS_ERR_TIMEOUT;
        (void)*((__IO uint8_t *)&SPIx->RXDR);
    }

    if (SPI_WaitFlag(SPIx, SPI_SR_EOT) != STATUS_OK) return STATUS_ERR_TIMEOUT;
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    return STATUS_OK;
}

Status_t SPI_TransmitReceive_Blocking(SPI_TypeDef *SPIx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size)
{
    SPI_PrepareTransfer(SPIx, size);
    SPIx->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;

    for (uint32_t i = 0; i < size; i++)
    {
        if (SPI_WaitFlag(SPIx, SPI_SR_TXP) != STATUS_OK) return STATUS_ERR_TIMEOUT;
        *((__IO uint8_t *)&SPIx->TXDR) = tx_data[i];

        if (SPI_WaitFlag(SPIx, SPI_SR_RXP) != STATUS_OK) return STATUS_ERR_TIMEOUT;
        rx_data[i] = *((__IO uint8_t *)&SPIx->RXDR);
    }

    if (SPI_WaitFlag(SPIx, SPI_SR_EOT) != STATUS_OK) return STATUS_ERR_TIMEOUT;
    SPIx->IFCR = SPI_IFCR_EOTC | SPI_IFCR_TXTFC;
    return STATUS_OK;
}


void SPI_TransmitReceive_BDMA(SPI_TypeDef *SPIx, BDMA_Channel_TypeDef *BDMA_Tx, BDMA_Channel_TypeDef *BDMA_Rx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size)
{
    /* RM0468 SPI DMA start sequence: RXDMAEN -> enable channels -> TXDMAEN -> SPE */
    SPI_PrepareTransfer(SPIx, size);
    SPIx->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    SPIx->CFG1 |= SPI_CFG1_RXDMAEN;

    /* Reload BDMA RX parameters (Peripheral to Memory) */
    BDMA_Rx->CCR &= ~BDMA_CCR_EN;
    BDMA_Rx->CPAR = (uint32_t)&SPIx->RXDR;
    BDMA_Rx->CM0AR = (uint32_t)rx_data; 
    BDMA_Rx->CNDTR = size;
    BDMA_Rx->CCR |= BDMA_CCR_EN;

    /* Reload BDMA TX parameters (Memory to Peripheral) */
    BDMA_Tx->CCR &= ~BDMA_CCR_EN;
    BDMA_Tx->CPAR = (uint32_t)&SPIx->TXDR;
    BDMA_Tx->CM0AR = (uint32_t)tx_data;
    BDMA_Tx->CNDTR = size;
    BDMA_Tx->CCR |= BDMA_CCR_EN;

    SPIx->CFG1 |= SPI_CFG1_TXDMAEN;
    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;
}

void SPI_AbortTransfer_BDMA(SPI_TypeDef *SPIx, BDMA_Channel_TypeDef *BDMA_Tx, BDMA_Channel_TypeDef *BDMA_Rx)
{
    /* Clearing SPE stops the master state machine and flushes the FIFOs, so the
       aborted frame cannot leak into the next transfer (RM0468 SPI disabling procedure) */
    SPIx->CR1 &= ~SPI_CR1_SPE;

    BDMA_Rx->CCR &= ~BDMA_CCR_EN;
    BDMA_Tx->CCR &= ~BDMA_CCR_EN;

    SPIx->IFCR = SPI_IFCR_ALL;
}


Status_t SPI_Transmit_DMA(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, const uint8_t *tx_data, uint32_t size)
{
    /* 1. SPI must be disabled before CFG1 register modification */
    SPIx->CR1 &= ~SPI_CR1_SPE;

    /* 2. Absolute clearing of all SPI status flags and errors */
    SPIx->IFCR = SPI_IFCR_ALL;

    /* 3. DMA stream configuration (D1/D2 Domain) */
    if (SPI_DisableDmaStream(DMA_Tx) != STATUS_OK) return STATUS_ERR_TIMEOUT;

    DMA_Tx->PAR  = (uint32_t)&SPIx->TXDR;
    DMA_Tx->M0AR = (uint32_t)tx_data;
    DMA_Tx->NDTR = size;
    
    /* Enable DMA stream (Hardware is now ready and waiting for a request from SPI) */
    DMA_Tx->CR |= DMA_SxCR_EN;

    /* 4. Connect TX DMA requests to SPI state machine and disable RX */
    SPIx->CFG1 |= SPI_CFG1_TXDMAEN; 
    SPIx->CFG1 &= ~SPI_CFG1_RXDMAEN; 

    /* 5. STM32H7 requires explicit specification of transaction size in Master mode */
    SPIx->CR2 = size;

    /* 6. Start hardware Master transaction - hardware will immediately trigger DMA */
    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;
    return STATUS_OK;
}

Status_t SPI_TransmitReceive_DMA(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, DMA_Stream_TypeDef *DMA_Rx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size)
{
    SPIx->CR1 &= ~SPI_CR1_SPE;
    SPIx->IFCR = SPI_IFCR_ALL;

    /* Drop DMA requests left over from the previous transfer, otherwise the RX stream
       drains the empty RXDR as soon as it is enabled (RM0468 SPI DMA sequence:
       RXDMAEN -> enable streams -> TXDMAEN -> SPE) */
    SPIx->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    SPIx->CFG1 |= SPI_CFG1_RXDMAEN;

    /* RX DMA Configuration */
    if (SPI_DisableDmaStream(DMA_Rx) != STATUS_OK) return STATUS_ERR_TIMEOUT;
    DMA_Rx->PAR = (uint32_t)&SPIx->RXDR;
    DMA_Rx->M0AR = (uint32_t)rx_data;
    DMA_Rx->NDTR = size;
    DMA_Rx->CR |= DMA_SxCR_EN;

    /* TX DMA Configuration */
    if (SPI_DisableDmaStream(DMA_Tx) != STATUS_OK) return STATUS_ERR_TIMEOUT;
    DMA_Tx->PAR = (uint32_t)&SPIx->TXDR;
    DMA_Tx->M0AR = (uint32_t)tx_data;
    DMA_Tx->NDTR = size;
    DMA_Tx->CR |= DMA_SxCR_EN;

    SPIx->CFG1 |= SPI_CFG1_TXDMAEN;
    SPIx->CR2 = size;

    SPIx->CR1 |= SPI_CR1_SPE;
    SPIx->CR1 |= SPI_CR1_CSTART;
    return STATUS_OK;
}

void SPI_CloseTransfer_DMA(SPI_TypeDef *SPIx)
{
    /* RM0468 DMA close sequence: disable the SPI, then drop the DMA requests */
    SPIx->CR1 &= ~SPI_CR1_SPE;
    SPIx->CFG1 &= ~(SPI_CFG1_RXDMAEN | SPI_CFG1_TXDMAEN);
    SPIx->IFCR = SPI_IFCR_ALL;
}
