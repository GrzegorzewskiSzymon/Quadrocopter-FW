/*
 * nrf24l01.h
 *
 *  Created on: May 8, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h723xx.h"
#include <stdint.h>
#include <stdbool.h>
#include "nrf24_payload.h"

/* ========================================================================= */
/* DATA TYPES & CONFIGURATION                                                */
/* ========================================================================= */

typedef enum {
    NRF_STATE_IDLE,
    NRF_STATE_TX_SPI_BUSY,
    NRF_STATE_TX_AIR_BUSY
} NRF24_State_t;

/* Hardware dependency injection structure (Dependency Injection) */
typedef struct {
    SPI_TypeDef         *SPIx;
    DMA_Stream_TypeDef  *DMA_Tx;
    DMA_Stream_TypeDef  *DMA_Rx;
} NRF24_HwConfig_t;


/* ========================================================================= */
/* PUBLIC API                                                                */
/* ========================================================================= */

void NRF24_Init(const NRF24_HwConfig_t *hw_config);
bool NRF24_Transmit_IT(const NRF24_Payload_t *payload);
NRF24_State_t NRF24_GetState(void);

/* --- HARDWARE CALLBACKS (CALLED FROM INTERRUPTS) --- */
void NRF24_DMA_RxComplete_Callback(void);
void NRF24_EXTI_Callback(void);