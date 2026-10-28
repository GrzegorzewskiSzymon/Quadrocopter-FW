/*
 * nrf24l01.c
 *
 *  Created on: May 8, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "nrf24l01.h"
#include "board_config.h"
#include "spi.h"
#include "gpio.h"
#include "stddef.h"
#include <string.h>
#include "systick.h"
#include "telemetry_queue.h"

/* --- MACROS --- */
#define NRF_CMD_W_REGISTER    0x20U
#define NRF_CMD_W_TX_PAYLOAD  0xA0U
#define NRF_CMD_FLUSH_TX      0xE1U

#define NRF_REG_CONFIG        0x00U
#define NRF_CONFIG_PWR_UP     (1U << 1U)
#define NRF_REG_EN_AA         0x01U
#define NRF_REG_SETUP_RETR    0x04U
#define NRF_REG_RF_CH         0x05U
#define NRF_REG_RF_SETUP      0x06U
#define NRF_REG_STATUS        0x07U
#define NRF_REG_RX_ADDR_P0    0x0AU
#define NRF_REG_TX_ADDR       0x10U
#define NRF_REG_RX_PW_P0      0x11U

#define NRF_STATUS_TX_DS      (1U << 5U)
#define NRF_STATUS_MAX_RT     (1U << 4U)

/* --- INTERNAL VARIABLES --- */
static volatile NRF24_State_t nrf_current_state = NRF_STATE_IDLE;

/* Pointers to MCU domain hardware (Dependency Injection) */
static SPI_TypeDef *nrf_spi = NULL;
static DMA_Stream_TypeDef *nrf_dma_tx = NULL;
static DMA_Stream_TypeDef *nrf_dma_rx = NULL;

/* DMA buffers - Aligned to D-Cache line (32 bytes) */
__attribute__((aligned(32), section(".sram4"))) static uint8_t dma_tx_buf[64]; 
__attribute__((aligned(32), section(".sram4"))) static uint8_t dma_rx_buf[64];

/* --- PRIVATE HELPER FUNCTIONS --- */

static Status_t NRF_SendCommand(uint8_t cmd)
{
    uint8_t status;
    GPIO_RESET(RF_CSN);
    Status_t result = SPI_TransmitReceive_Blocking(nrf_spi, &cmd, &status, 1U);
    GPIO_SET(RF_CSN);
    return result;
}

static Status_t NRF_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { NRF_CMD_W_REGISTER | (reg & 0x1FU), value };
    uint8_t rx[2];
    GPIO_RESET(RF_CSN);
    Status_t result = SPI_TransmitReceive_Blocking(nrf_spi, tx, rx, 2U);
    GPIO_SET(RF_CSN);
    return result;
}

static Status_t NRF_WriteRegMulti(uint8_t reg, const uint8_t *data, uint8_t size)
{
    uint8_t buffer[6]; 
    buffer[0] = NRF_CMD_W_REGISTER | (reg & 0x1FU);
    for(uint8_t i = 0; i < size; i++) 
    {
        buffer[i + 1U] = data[i];
    }
    
    GPIO_RESET(RF_CSN);
    Status_t result = SPI_Transmit_Blocking(nrf_spi, buffer, size + 1U);
    GPIO_SET(RF_CSN);
    return result;
}

static Status_t NRF_ReadReg(uint8_t reg, uint8_t *value)
{
    uint8_t tx[2] = { (reg & 0x1FU), 0x00U };
    uint8_t rx[2] = { 0U };
    GPIO_RESET(RF_CSN);
    Status_t result = SPI_TransmitReceive_Blocking(nrf_spi, tx, rx, 2U);
    GPIO_SET(RF_CSN);
    *value = rx[1];
    return result;
}

/* Each step runs only if all previous ones succeeded; the first error is returned */
static Status_t NRF24_Setup(void)
{
    /* 1. System startup (PWR_UP=1, PRIM_RX=0 -> PTX), NO CRC */
    Status_t status = NRF_WriteReg(NRF_REG_CONFIG, NRF_CONFIG_PWR_UP);
    
    /* Time for T_pd2stby (max 1.5ms) - defined in external delay module */
    Delay_ms(2U); 

    /* 2. Disable Auto-ACK and retransmission */
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_EN_AA, 0x00U);
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_SETUP_RETR, 0x00U);
    
    /* 3. RF Configuration: Channel 76 (2476 MHz) */
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_RF_CH, 76U);
    
    /* 4. RF SETUP Configuration: 1 Mbps, power 0 dBm (0x06) */
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_RF_SETUP, 0x06U);

    /* 5. Pipe Address Configuration */
    const uint8_t pipe_addr[5] = {'D', 'R', 'O', 'N', '1'};
    if (status == STATUS_OK) status = NRF_WriteRegMulti(NRF_REG_TX_ADDR, pipe_addr, 5U);
    if (status == STATUS_OK) status = NRF_WriteRegMulti(NRF_REG_RX_ADDR_P0, pipe_addr, 5U); 
    
    /* 6. Set fixed payload size for channel 0 to 32 bytes */
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_RX_PW_P0, 32U); 

    /* 7. Clear TX buffer and old interrupt flags before start */
    if (status == STATUS_OK) status = NRF_SendCommand(NRF_CMD_FLUSH_TX);
    if (status == STATUS_OK) status = NRF_WriteReg(NRF_REG_STATUS, NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);

    return status;
}

/* --- PUBLIC API IMPLEMENTATION --- */

Status_t NRF24_Init(const NRF24_HwConfig_t *hw_config)
{
    /* 1. Dependency Injection */
    nrf_spi = hw_config->SPIx;
    nrf_dma_tx = hw_config->DMA_Tx;
    nrf_dma_rx = hw_config->DMA_Rx;
    
    /* 2. Register configuration */
    Status_t status = NRF24_Setup();
    nrf_current_state = (status == STATUS_OK) ? NRF_STATE_IDLE : NRF_STATE_FAULT;

    return status;
}

Status_t NRF24_Transmit_IT(const NRF24_Payload_t *payload)
{
    if (nrf_current_state != NRF_STATE_IDLE) {
        return STATUS_ERR_BUSY;
    }
    nrf_current_state = NRF_STATE_TX_SPI_BUSY;

    /* DMA buffer preparation: [COMMAND] [32 BYTES PAYLOAD] */
    dma_tx_buf[0] = NRF_CMD_W_TX_PAYLOAD;
    
    /* Fixed-size copy is inlined by GCC; &dma_tx_buf[1] is unaligned, so word pointer
       casts could be merged into STRD/STM, which fault on unaligned addresses */
    memcpy(&dma_tx_buf[1], payload, sizeof(NRF24_Payload_t));

    SCB_CleanDCache_by_Addr((uint32_t*)dma_tx_buf, 33U);

    GPIO_RESET(RF_CSN);
    Status_t status = SPI_TransmitReceive_DMA(nrf_spi, nrf_dma_tx, nrf_dma_rx, dma_tx_buf, dma_rx_buf, 33U);
    if (status != STATUS_OK)
    {
        /* A DMA stream that does not stop is a hardware fault, not a busy radio */
        GPIO_SET(RF_CSN);
        nrf_current_state = NRF_STATE_FAULT;
    }

    return status;
}

/* --- STATE MACHINE INTERRUPT HANDLING --- */

/**
 * @trigger DMA1 Stream2 (D2 Domain) (SPI3 RXNE event) transfer complete (NRF24 TX payload sent).
 */
void NRF24_DMA_RxComplete_Callback(void)
{
    /* 1. SPI transaction completed - close CSN barrier */
    GPIO_SET(RF_CSN);
    SPI_CloseTransfer_DMA(nrf_spi);

    /* 2. Transition to waiting mode for the air */
    nrf_current_state = NRF_STATE_TX_AIR_BUSY;

    /* 3. Enable PA RFX2401C and pulse CE */
    GPIO_SET(RF_TXEN);
    GPIO_RESET(RF_RXEN);
    
}

void NRF24_EXTI_Callback(void)
{
    /* 1. Immediate PA shutdown due to power consumption */
    GPIO_RESET(RF_TXEN);

    /* 2. Read status register */
    uint8_t status = 0U;
    Status_t result = NRF_ReadReg(NRF_REG_STATUS, &status);

    /* 3. Clear interrupt flags on NRF chip */
    if (result == STATUS_OK) result = NRF_WriteReg(NRF_REG_STATUS, (NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));

    /* Optional: Clear buffer in case of MAX_RT retransmission error */
    if ((result == STATUS_OK) && (status & NRF_STATUS_MAX_RT)) {
        result = NRF_SendCommand(NRF_CMD_FLUSH_TX);
    }

    if (result != STATUS_OK)
    {
        /* Flags left set keep IRQ low, so no further edge would ever arrive: stop the TX chain */
        nrf_current_state = NRF_STATE_FAULT;
        return;
    }

    /* 4. Return to IDLE - state machine ready for new frame from Scheduler */
    nrf_current_state = NRF_STATE_IDLE;

    NRF24_Payload_t next_payload;
    if (TelemetryQueue_Pop(&next_payload))
    {
        /* Telemetry is best effort: a failed start is latched as NRF_STATE_FAULT */
        (void)NRF24_Transmit_IT(&next_payload);
    }
}

NRF24_State_t NRF24_GetState(void)
{
    return nrf_current_state;
}
