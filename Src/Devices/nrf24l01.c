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
static volatile NRF24_State_t current_state = NRF_STATE_IDLE;

/* Pointers to MCU domain hardware (Dependency Injection) */
static SPI_TypeDef *nrf_spi = NULL;
static DMA_Stream_TypeDef *nrf_dma_tx = NULL;
static DMA_Stream_TypeDef *nrf_dma_rx = NULL;

/* DMA buffers - Aligned to D-Cache line (32 bytes) */
__attribute__((aligned(32), section(".sram4"))) static uint8_t dma_tx_buf[64]; 
__attribute__((aligned(32), section(".sram4"))) static uint8_t dma_rx_buf[64];

/* --- PRIVATE HELPER FUNCTIONS --- */

static void NRF_SendCommand(uint8_t cmd)
{
    uint8_t status;
    GPIO_RESET(RF_CSN);
    SPI_TransmitReceive_Blocking(nrf_spi, &cmd, &status, 1U);
    GPIO_SET(RF_CSN);
}

static void NRF_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx[2] = { NRF_CMD_W_REGISTER | (reg & 0x1FU), value };
    uint8_t rx[2];
    GPIO_RESET(RF_CSN);
    SPI_TransmitReceive_Blocking(nrf_spi, tx, rx, 2U);
    GPIO_SET(RF_CSN);
}

static void NRF_WriteRegMulti(uint8_t reg, const uint8_t *data, uint8_t size)
{
    uint8_t buffer[6]; 
    buffer[0] = NRF_CMD_W_REGISTER | (reg & 0x1FU);
    for(uint8_t i = 0; i < size; i++) 
    {
        buffer[i + 1U] = data[i];
    }
    
    GPIO_RESET(RF_CSN);
    SPI_Transmit_Blocking(nrf_spi, buffer, size + 1U);
    GPIO_SET(RF_CSN);
}

static uint8_t NRF_ReadReg(uint8_t reg)
{
    uint8_t tx[2] = { (reg & 0x1FU), 0x00U };
    uint8_t rx[2];
    GPIO_RESET(RF_CSN);
    SPI_TransmitReceive_Blocking(nrf_spi, tx, rx, 2U);
    GPIO_SET(RF_CSN);
    return rx[1];
}

static void NRF24_Setup(void)
{
    /* 1. System startup (PWR_UP=1, PRIM_RX=0 -> PTX), NO CRC */
    NRF_WriteReg(NRF_REG_CONFIG, NRF_CONFIG_PWR_UP);
    
    /* Time for T_pd2stby (max 1.5ms) - defined in external delay module */
    extern void NRF24_HW_Delay_us(uint32_t us);
    NRF24_HW_Delay_us(2000U); 

    /* 2. Disable Auto-ACK and retransmission */
    NRF_WriteReg(NRF_REG_EN_AA, 0x00U);
    NRF_WriteReg(NRF_REG_SETUP_RETR, 0x00U);
    
    /* 3. RF Configuration: Channel 76 (2476 MHz) */
    NRF_WriteReg(NRF_REG_RF_CH, 76U);
    
    /* 4. RF SETUP Configuration: 1 Mbps, power 0 dBm (0x06) */
    NRF_WriteReg(NRF_REG_RF_SETUP, 0x06U);

    /* 5. Pipe Address Configuration */
    const uint8_t pipe_addr[5] = {'D', 'R', 'O', 'N', '1'};
    NRF_WriteRegMulti(NRF_REG_TX_ADDR, pipe_addr, 5U);
    NRF_WriteRegMulti(NRF_REG_RX_ADDR_P0, pipe_addr, 5U); 
    
    /* 6. Set fixed payload size for channel 0 to 32 bytes */
    NRF_WriteReg(NRF_REG_RX_PW_P0, 32U); 

    /* 7. Clear TX buffer and old interrupt flags before start */
    NRF_SendCommand(NRF_CMD_FLUSH_TX);
    NRF_WriteReg(NRF_REG_STATUS, NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT);
}

/* --- PUBLIC API IMPLEMENTATION --- */

void NRF24_Init(const NRF24_HwConfig_t *hw_config)
{
    /* 1. Dependency Injection */
    nrf_spi = hw_config->SPIx;
    nrf_dma_tx = hw_config->DMA_Tx;
    nrf_dma_rx = hw_config->DMA_Rx;
    
    current_state = NRF_STATE_IDLE;
    
    /* 2. Register configuration */
    NRF24_Setup();
}

bool NRF24_Transmit_IT(const NRF24_Payload_t *payload)
{
    if (current_state != NRF_STATE_IDLE) {
        return false; /* Protection against overwriting in flight */
    }

    current_state = NRF_STATE_TX_SPI_BUSY;

/* DMA buffer preparation: [COMMAND] [32 BYTES PAYLOAD] */
    dma_tx_buf[0] = NRF_CMD_W_TX_PAYLOAD;
    const uint8_t *p_data = (const uint8_t *)payload;
    
    /* Compile-time evaluation of struct size */
    const uint8_t payload_len = (uint8_t)sizeof(NRF24_Payload_t);
    uint8_t i = 0U;

    /* 1. Copy valid payload data (preventing out-of-bounds read) */
    for (; i < payload_len; i++) {
        dma_tx_buf[i + 1U] = p_data[i];
    }

    /* 2. Pad the remaining buffer with zeros to strictly meet 32-byte width */
    for (; i < 32U; i++) {
        dma_tx_buf[i + 1U] = 0x00U;
    }

    /* Push CPU changes from D-Cache to Main Memory (SRAM4) */
    SCB_CleanDCache_by_Addr((uint32_t*)dma_tx_buf, 33U);

    /* Open SPI session */
    GPIO_RESET(RF_CSN);

    /* Trigger asynchronous transfer in background */
    SPI_TransmitReceive_DMA(nrf_spi, nrf_dma_tx, nrf_dma_rx, dma_tx_buf, dma_rx_buf, 33U);

    return true;
}

/* --- STATE MACHINE INTERRUPT HANDLING --- */

void NRF24_DMA_RxComplete_Callback(void)
{
    /* 1. SPI transaction completed - close CSN barrier */
    GPIO_SET(RF_CSN);

    /* 2. Transition to waiting mode for the air */
    current_state = NRF_STATE_TX_AIR_BUSY;

    /* 3. Enable PA RFX2401C and pulse CE */
    GPIO_SET(RF_TXEN);
    GPIO_RESET(RF_RXEN);
    
    GPIO_SET(RF_CE);
    
    extern void NRF24_HW_Delay_us(uint32_t us);
    NRF24_HW_Delay_us(15U); 
    
    GPIO_RESET(RF_CE);
}

void NRF24_EXTI_Callback(void)
{
    /* 1. Immediate PA shutdown due to power consumption */
    GPIO_RESET(RF_TXEN);

    /* 2. Read status register */
    uint8_t status = NRF_ReadReg(NRF_REG_STATUS);

    /* 3. Clear interrupt flags on NRF chip */
    NRF_WriteReg(NRF_REG_STATUS, (NRF_STATUS_TX_DS | NRF_STATUS_MAX_RT));

    /* Optional: Clear buffer in case of MAX_RT retransmission error */
    if (status & NRF_STATUS_MAX_RT) {
        NRF_SendCommand(NRF_CMD_FLUSH_TX);
    }

    /* 4. Return to IDLE - state machine ready for new frame from Scheduler */
    current_state = NRF_STATE_IDLE;
}

NRF24_State_t NRF24_GetState(void)
{
    return current_state;
}