/*
 * led.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "led.h"
#include "spi.h"
#include "gpio.h"
#include "board_config.h"

#define BITS_PER_LED       24U
#define SPI_TX_SIZE         100U 
#define WS2812_0_CODE       0xC0U 
#define WS2812_1_CODE       0xFCU 

static uint8_t spi_tx_buffer[SPI_TX_SIZE];

static void LED_Signal_Init(void)
{
	GPIO_InitPin(PORT(LED_RED),    PIN(LED_RED),    GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_YELLOW), PIN(LED_YELLOW), GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_GREEN),  PIN(LED_GREEN),  GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_BLUE),   PIN(LED_BLUE),   GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);

	/* Ensure all LEDs are turned off at startup */
	GPIO_RESET(LED_RED);
	GPIO_RESET(LED_YELLOW);
	GPIO_RESET(LED_GREEN);
	GPIO_RESET(LED_BLUE);
}

static void LED_Addressable_Init(void)
{
   /* Pin initialization using macros from board_config.h and gpio.h */
    GPIO_INIT(LED_MOSI, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_PD);
    GPIO_INIT_AF(LED_MOSI, 5U);

    GPIO_INIT(LED_SCK, GPIO_MODE_AF, GPIO_OTYPE_PP, GPIO_SPEED_VHIGH, GPIO_PUPD_NONE);
    GPIO_INIT_AF(LED_SCK, 5U);

    /* SPI4 configuration */
    SPI_Config_t led_spi_cfg = {
        .Mode = SPI_MODE_MASTER,
        .Direction = SPI_DIR_SIMPLEX_TX,
        .Prescaler = (4U << SPI_CFG1_MBR_Pos),
        .DataSize = 8U,
        .CPOL = false,
        .CPHA = true 
    };
    SPI_Init(SPI4, &led_spi_cfg);

    /* Buffer zeroing (Reset Latch padding) */
    for (uint32_t i = 0; i < SPI_TX_SIZE; i++)
    {
        spi_tx_buffer[i] = 0x00U;
    }
}

void LED_Init(void)
{
	
	LED_Signal_Init();	

	LED_Addressable_Init();
}

void LED_SetColor(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= LED_COUNT) return;

    uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    uint32_t buffer_offset = led_index * BITS_PER_LED;

    for (uint8_t i = 0; i < 24U; i++)
    {
        if ((color & (1U << (23U - i))) != 0U)
            spi_tx_buffer[buffer_offset + i] = WS2812_1_CODE;
        else
            spi_tx_buffer[buffer_offset + i] = WS2812_0_CODE;
    }
}

void LED_Update(void)
{
    SPI_Transmit_Blocking(SPI4, spi_tx_buffer, SPI_TX_SIZE);
}

