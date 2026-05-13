/*
 * spi.h
 *
 *  Created on: Mar 20, 2026
 *      Author: Szymon Grzegorzewski
 */
#pragma once

#include "stm32h723xx.h"
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SPI_MODE_MASTER,
    SPI_MODE_SLAVE
} SPI_Mode_t;

typedef enum {
    SPI_DIR_FULL_DUPLEX,
    SPI_DIR_SIMPLEX_TX,
    SPI_DIR_SIMPLEX_RX,
    SPI_DIR_HALF_DUPLEX
} SPI_Direction_t;

typedef struct {
    SPI_Mode_t      Mode;
    SPI_Direction_t Direction;
    uint32_t        Prescaler;
    uint8_t         DataSize;
    bool            CPOL;
    bool            CPHA;
} SPI_Config_t;

void SPI_Init(SPI_TypeDef *SPIx, const SPI_Config_t *config);
void SPI_Transmit_Blocking(SPI_TypeDef *SPIx, const uint8_t *data, uint32_t size);
void SPI_TransmitReceive_Blocking(SPI_TypeDef *SPIx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size);

void SPI_TransmitReceive_DMA(SPI_TypeDef *SPIx, BDMA_Channel_TypeDef *BDMA_Tx, BDMA_Channel_TypeDef *BDMA_Rx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size);

/* Function signature for D1/D2 domain (SPI1, SPI2, SPI3, SPI4, SPI5) using DMA1/DMA2 */
void SPI_Transmit_DMA(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, const uint8_t *tx_data, uint32_t size);

/* Additional function for standard DMA streams (D1/D2 Domain) */
void SPI_TransmitReceive_DMA_Stream(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, DMA_Stream_TypeDef *DMA_Rx, const uint8_t *tx_data, uint8_t *rx_data, uint32_t size);