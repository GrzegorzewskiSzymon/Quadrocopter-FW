/*
 * nrf24l01.c
 *
 *  Created on: May 8, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "nrf24l01.h"
#include "board_config.h"

/* --- MACROS --- */
#define NRF_CMD_W_REGISTER    0x20U
#define NRF_CMD_W_TX_PAYLOAD  0xA0U
#define NRF_CMD_FLUSH_TX      0xE1U

#define NRF_REG_STATUS        0x07U
#define NRF_STATUS_TX_DS      (1U << 5U)
#define NRF_STATUS_MAX_RT     (1U << 4U)

/* --- INTERNAL VARIABLES --- */
static volatile NRF24_State_t current_state = NRF_STATE_IDLE;

/* Pointers to MCU domain hardware */
static SPI_TypeDef *nrf_spi = NULL;
static DMA_Stream_TypeDef *nrf_dma_tx = NULL;
static DMA_Stream_TypeDef *nrf_dma_rx = NULL;

/* DMA buffers - Aligned to D-Cache line (32 bytes) */
__attribute__((aligned(32))) static uint8_t dma_tx_buf[64]; 
__attribute__((aligned(32))) static uint8_t dma_rx_buf[64];

/* Helper functions for registers (blocking, only for Initialization/EXTI) */
static void NRF_SendCommand(uint8_t cmd)
{
    uint8_t status;
    GPIO_RESET(RF_CSN);
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
    /* 1. System startup (PWR_UP=1, PRIM_RX=0 -> PTX), NO CRC (according to RPi test) */
    NRF_WriteReg(NRF_REG_CONFIG, NRF_CONFIG_PWR_UP);
    NRF24_HW_Delay_us(2000U); /* Time for T_pd2stby (max 1.5ms) */

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

/* --- API IMPLEMENTATION --- */

void NRF24_Init(SPI_TypeDef *SPIx, DMA_Stream_TypeDef *DMA_Tx, DMA_Stream_TypeDef *DMA_Rx)
{
    /* 1. ENABLE CLOCKS (GPIO A, C, D & SPI3) */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    (void)RCC->AHB4ENR; 
    
    RCC->APB1LENR |= RCC_APB1LENR_SPI3EN;
    (void)RCC->APB1LENR; 

/* --- 2. GPIO CONFIGURATION (SPI3 AF6: PC10, PC11, PC12) --- */
    GPIO_INIT(RF_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_SCK, 6U);

    GPIO_INIT(RF_MISO, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_MISO, 6U);

    GPIO_INIT(RF_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(RF_MOSI, 6U);

    /* --- 3. GPIO CONFIGURATION (CONTROL PINS) --- */
    /* CSN - Chip Select Not (PD0) */
    GPIO_INIT(RF_CSN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_SET(RF_CSN);   /* Standby state (High) */

    /* CE - Chip Enable (PA15) */
    GPIO_INIT(RF_CE, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_CE);  /* Idle state (Low) */

    /* TXEN - PA Transmit Enable (PD2) */
    GPIO_INIT(RF_TXEN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_TXEN); /* Idle state (Low) */

    /* RXEN - LNA Receive Enable (PD4) */
    GPIO_INIT(RF_RXEN, GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_RESET(RF_RXEN); /* Idle state (Low) */

    GPIO_INIT(RF_IRQ, GPIO_MODE_INPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PU);

    /* Idle states: CSN = 1, CE = 0, TXEN = 0, RXEN = 0 */
    GPIOD->BSRR = (1U << 0U);         /* NRF_CSN_HIGH */
    GPIOA->BSRR = (1U << (15U + 16U));/* NRF_CE_LOW */
    GPIOD->BSRR = (1U << (2U + 16U)); /* RFX_TXEN_LOW */
    GPIOD->BSRR = (1U << (4U + 16U)); /* RFX_RXEN_LOW */

    /* 4. SPI3 INITIALIZATION */
    /* APB1 = 137.5 MHz. MBR = 100b (4) -> Div 32. SPI Clock = 4.29 MHz. */
    SPI_Config_t spi3_cfg = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIR_FULL_DUPLEX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos), 
        .DataSize = 8,
        .CPOL = false,
        .CPHA = false
    };
    SPI_Init(SPI3, &spi3_cfg);

    /* 5. EXTI CONFIGURATION FOR PD1 */
    GPIO_NRF_EXTI_Init();


    nrf_spi = SPIx;
    nrf_dma_tx = DMA_Tx;
    nrf_dma_rx = DMA_Rx;
    current_state = NRF_STATE_IDLE;
    
    /* Register configuration (similar to test) called here 
       or from external configuration function */
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
    for (uint8_t i = 0; i < 32U; i++) {
        dma_tx_buf[i + 1U] = p_data[i];
    }

    /* ---- ADD THIS ----
       Push data from L1 D-Cache to physical SRAM memory.
       We're flushing 33 bytes (command + payload). */
    SCB_CleanDCache_by_Addr((uint32_t*)dma_tx_buf, 33U);

    /* Open SPI session */
    GPIO_RESET(RF_CSN);

    /* Trigger asynchronous transfer in background */
    SPI_TransmitReceive_DMA_Stream(nrf_spi, nrf_dma_tx, nrf_dma_rx, dma_tx_buf, dma_rx_buf, 33U);

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
    NRF24_HW_Delay_us(15U); /* In IT 15us (8k cycles at 550MHz) is a fraction of a percent load, completely safe */
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