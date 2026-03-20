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
