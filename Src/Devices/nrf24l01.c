/*
 * nrf24l01.c
 *
 *  Created on: May 8, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "stm32h723xx.h"
#include "spi.h"

/* --- NRF24L01 MACROS --- */
#define NRF_CMD_W_REGISTER    0x20U
#define NRF_CMD_FLUSH_TX      0xE1U
#define NRF_CMD_W_TX_PAYLOAD  0xA0U
#define NRF_CMD_NOP           0xFFU

#define NRF_REG_CONFIG        0x00U
#define NRF_REG_EN_AA         0x01U
#define NRF_REG_SETUP_RETR    0x04U
#define NRF_REG_RF_CH         0x05U
#define NRF_REG_STATUS        0x07U

#define NRF_REG_RF_SETUP      0x06U
#define NRF_REG_RX_ADDR_P0    0x0AU
#define NRF_REG_TX_ADDR       0x10U
#define NRF_REG_RX_PW_P0      0x11U

#define NRF_CONFIG_PWR_UP     (1U << 1U)
#define NRF_CONFIG_PRIM_RX    (1U << 0U)
#define NRF_STATUS_TX_DS      (1U << 5U)

/* --- HARDWARE ABSTRACTION MACROS --- */
#define NRF_CSN_LOW()         (GPIOD->BSRR = (1U << 16U))
#define NRF_CSN_HIGH()        (GPIOD->BSRR = (1U << 0U))

#define NRF_CE_LOW()          (GPIOA->BSRR = (1U << (15U + 16U)))
#define NRF_CE_HIGH()         (GPIOA->BSRR = (1U << 15U))

#define RFX_TXEN_LOW()        (GPIOD->BSRR = (1U << (2U + 16U)))
#define RFX_TXEN_HIGH()       (GPIOD->BSRR = (1U << 2U))

#define RFX_RXEN_LOW()        (GPIOD->BSRR = (1U << (4U + 16U)))
#define RFX_RXEN_HIGH()       (GPIOD->BSRR = (1U << 4U))

/**
 * @brief Simple delay based on a loop.
 * Until the hardware timer is connected, it is sufficient for blocking the core for a few microseconds.
 */
static void NRF_Delay_us(volatile uint32_t microseconds)
{
    /* Assuming 550 MHz core, 1 us is 550 cycles. Divide by approximate loop overhead. */
    microseconds *= (550U / 3U);
    while (microseconds--) { __NOP(); }
}

/**
 * @brief Write to NRF register.
 */
static uint8_t NRF_WriteReg(uint8_t reg, uint8_t value)
{
    uint8_t tx_data[2] = { NRF_CMD_W_REGISTER | (reg & 0x1FU), value };
    uint8_t rx_data[2] = { 0U, 0U };

    NRF_CSN_LOW();
    SPI_TransmitReceive_Blocking(SPI3, tx_data, rx_data, 2U);
    NRF_CSN_HIGH();

    return rx_data[0]; /* Return STATUS byte obtained when sending the first frame */
}

/**
 * @brief Send raw command.
 */
static uint8_t NRF_SendCommand(uint8_t cmd)
{
    uint8_t status = 0U;

    NRF_CSN_LOW();
    SPI_TransmitReceive_Blocking(SPI3, &cmd, &status, 1U);
    NRF_CSN_HIGH();

    return status;
}

/**
 * @brief Hardware initialization of GPIO and SPI3 for NRF24L01.
 */
void NRF24_Test_Init(void)
{
    /* 1. ENABLE CLOCKS (GPIO A, C, D & SPI3) */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN;
    (void)RCC->AHB4ENR; /* DSB substitute */
    
    RCC->APB1LENR |= RCC_APB1LENR_SPI3EN;
    (void)RCC->APB1LENR; 

    /* 2. GPIO CONFIGURATION (SPI3 AF6: PC10, PC11, PC12) */
    /* Set Alternate Function mode (10b) */
    GPIOC->MODER = (GPIOC->MODER & ~(GPIO_MODER_MODE10_Msk | GPIO_MODER_MODE11_Msk | GPIO_MODER_MODE12_Msk)) |
                   (2U << GPIO_MODER_MODE10_Pos) | (2U << GPIO_MODER_MODE11_Pos) | (2U << GPIO_MODER_MODE12_Pos);
    
    /* Set High Speed (10b) */
    GPIOC->OSPEEDR = (GPIOC->OSPEEDR & ~(GPIO_OSPEEDR_OSPEED10_Msk | GPIO_OSPEEDR_OSPEED11_Msk | GPIO_OSPEEDR_OSPEED12_Msk)) |
                     (2U << GPIO_OSPEEDR_OSPEED10_Pos) | (2U << GPIO_OSPEEDR_OSPEED11_Pos) | (2U << GPIO_OSPEEDR_OSPEED12_Pos);

    /* Set AF6 */
    GPIOC->AFR[1] = (GPIOC->AFR[1] & ~(GPIO_AFRH_AFSEL10_Msk | GPIO_AFRH_AFSEL11_Msk | GPIO_AFRH_AFSEL12_Msk)) |
                    (6U << GPIO_AFRH_AFSEL10_Pos) | (6U << GPIO_AFRH_AFSEL11_Pos) | (6U << GPIO_AFRH_AFSEL12_Pos);

    /* 3. GPIO CONFIGURATION (CONTROL PINS) */
    /* Outputs (01b): CSN (PD0), CE (PA15), TXEN (PD2), RXEN (PD4) */
    GPIOD->MODER = (GPIOD->MODER & ~(GPIO_MODER_MODE0_Msk | GPIO_MODER_MODE2_Msk | GPIO_MODER_MODE4_Msk)) |
                   (1U << GPIO_MODER_MODE0_Pos) | (1U << GPIO_MODER_MODE2_Pos) | (1U << GPIO_MODER_MODE4_Pos);
    GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE15_Msk) | (1U << GPIO_MODER_MODE15_Pos);

    /* Default states: CSN = 1, CE = 0, TXEN = 0, RXEN = 0 */
    NRF_CSN_HIGH();
    NRF_CE_LOW();
    RFX_TXEN_LOW();
    RFX_RXEN_LOW();

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
}


/**
 * @brief Write to multi-byte registers (e.g., address configuration).
 */
static void NRF_WriteRegMulti(uint8_t reg, const uint8_t *data, uint8_t size)
{
    /* Command is 1 byte, address max 5 bytes -> 6-byte buffer */
    uint8_t buffer[6]; 
    buffer[0] = NRF_CMD_W_REGISTER | (reg & 0x1FU);
    
    for(uint8_t i = 0; i < size; i++) 
    {
        buffer[i + 1] = data[i];
    }
    
    NRF_CSN_LOW();
    /* Using direct blocking transmission from your stateless SPI */
    SPI_Transmit_Blocking(SPI3, buffer, size + 1U);
    NRF_CSN_HIGH();
}


/**
 * @brief Infinite test loop sending raw packets.
 */
void NRF24_Test_Run(void)
{
    /* 1. Power up device (PWR_UP=1, PRIM_RX=0 -> PTX) */
    NRF_WriteReg(NRF_REG_CONFIG, NRF_CONFIG_PWR_UP);
    NRF_Delay_us(2000U); /* Time for T_pd2stby */

    /* 2. Safe test - disable Auto-ACK and retransmission on STM32 */
    NRF_WriteReg(NRF_REG_EN_AA, 0x00U);
    NRF_WriteReg(NRF_REG_SETUP_RETR, 0x00U);
    
    /* 3. RF Configuration: Channel 76 (2476 MHz) */
    NRF_WriteReg(NRF_REG_RF_CH, 76U);
    
    /* 4. RF SETUP Configuration: 1 Mbps, 0 dBm power.
       On boards with external RFX2401C, standard settings often enforce 1 Mbps
       for band stability. Value 0x06 (Bit 3=0, Bit 5=0 -> 1 Mbps). */
    NRF_WriteReg(NRF_REG_RF_SETUP, 0x06U);

    /* 5. Pipe Address Configuration */
    const uint8_t pipe_addr[5] = {'D', 'R', 'O', 'N', '1'}; // b"DRON1"
    NRF_WriteRegMulti(NRF_REG_TX_ADDR, pipe_addr, 5U);
    /* It is good engineering practice to set the same address on the RX channel P0.
       When we enable Auto-ACK in the future, the ACK will come on Pipe 0. */
    NRF_WriteRegMulti(NRF_REG_RX_ADDR_P0, pipe_addr, 5U); 
    
    /* Set fixed payload size for channel 0 to 32 bytes */
    NRF_WriteReg(NRF_REG_RX_PW_P0, 32U); 

    /* 6. Prepare 32-byte TX buffer */
    uint8_t payload[33];
    payload[0] = NRF_CMD_W_TX_PAYLOAD;
    for(uint8_t i = 1; i <= 32; i++) 
    {
        payload[i] = i; /* Test pattern 1..32 */
    }

    for (;;)
    {
        /* Clear TX buffer */
        NRF_SendCommand(NRF_CMD_FLUSH_TX);

        /* Clear interrupt flags from previous loop (Write-1-to-clear) */
        /* NRF_STATUS_TX_DS (bit 5) and just in case MAX_RT (bit 4) */
        NRF_WriteReg(NRF_REG_STATUS, (1U << 5U) | (1U << 4U)); 

        /* Load payload to device in one CSN pull */
        NRF_CSN_LOW();
        SPI_Transmit_Blocking(SPI3, payload, 33U);
        NRF_CSN_HIGH();

        /* Activate RF amplifier (TX only) */
        RFX_TXEN_HIGH();
        RFX_RXEN_LOW();
        NRF_Delay_us(2U); /* PA stabilization in RFX module */

        /* CE pulse triggering transmission from buffer */
        NRF_CE_HIGH();
        NRF_Delay_us(15U);
        NRF_CE_LOW();

        /* Polling hardware TX_DS flag */
        uint8_t status;
        do {
            status = NRF_SendCommand(NRF_CMD_NOP);
        } while ((status & (1U << 5U)) == 0);

        /* Disable TX amplifier */
        RFX_TXEN_LOW();

        /* Delay between packets for RPi test - 50 ms */
        NRF_Delay_us(50000U); 
    }
}