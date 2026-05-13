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
#include "spi.h"
#include <stddef.h>
#include "gpio.h"
#include "board_config.h"

/* --- MACROS --- */
#define NRF_CMD_W_REGISTER    0x20U
#define NRF_CMD_W_TX_PAYLOAD  0xA0U
#define NRF_CMD_FLUSH_TX      0xE1U

#define NRF_REG_CONFIG        0x00U
#define NRF_REG_EN_AA         0x01U
#define NRF_REG_SETUP_RETR    0x04U
#define NRF_REG_RF_CH         0x05U
#define NRF_REG_RF_SETUP      0x06U
#define NRF_REG_STATUS        0x07U
#define NRF_REG_RX_ADDR_P0    0x0AU
#define NRF_REG_TX_ADDR       0x10U
#define NRF_REG_RX_PW_P0      0x11U

#define NRF_CONFIG_PWR_UP     (1U << 1U)
#define NRF_STATUS_TX_DS      (1U << 5U)
#define NRF_STATUS_MAX_RT     (1U << 4U)

/* --- FLIGHT CONTROLLER DATA STRUCTURE (Exactly 32 bytes) --- */
typedef struct __attribute__((packed)) {
    uint16_t aileron;      /* Roll */
    uint16_t elevator;     /* Pitch */
    uint16_t throttle;     /* Thrust */
    uint16_t rudder;       /* Yaw */
    uint8_t  switches_a;   /* Arming, Flight Mode */
    uint8_t  switches_b;   /* Beeper, OSD */
    uint8_t  telemetry_req;/* Telemetry request in ACK */
    uint8_t  reserved[21]; /* Padding to 32 bytes */
} NRF24_Payload_t;

/* --- NRF STATE MACHINE STATES --- */
typedef enum {
    NRF_STATE_IDLE,
    NRF_STATE_TX_SPI_BUSY, /* DMA transmits data over SPI */
    NRF_STATE_TX_AIR_BUSY, /* Waiting for EXTI (packet in the air) */
    NRF_STATE_ERROR
} NRF24_State_t;

extern void NRF24_HW_Delay_us(uint32_t us);

/* --- MAIN API --- */
void NRF24_Init(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, DMA_Stream_TypeDef *DMA_Rx);
bool NRF24_Transmit_IT(const NRF24_Payload_t *payload);

/* --- HARDWARE CALLBACKS (CALLED FROM INTERRUPTS) --- */
void NRF24_DMA_RxComplete_Callback(void);
void NRF24_EXTI_Callback(void);

NRF24_State_t NRF24_GetState(void);