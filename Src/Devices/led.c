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
#define SPI_TX_SIZE         128U 
#define WS2812_0_CODE       0xC0U 
#define WS2812_1_CODE       0xFCU 
#define WS2812_PREAMBLE_BYTES 16U

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
    
    /*
     * CRITICAL: Hardware SPI activation (CSTART/SPE) introduces jitter on the very first 
     * transmitted byte. Since the WS2812 protocol relies on strict pulse-width timing 
     * rather than a clock line, this initial jitter corrupts the first LED's data frame. 
     * Shifting the payload with a zero-byte preamble allows the SPI hardware pipeline 
     * to fully stabilize before transmitting the actual color data.
     */
    uint32_t buffer_offset = WS2812_PREAMBLE_BYTES + (led_index * BITS_PER_LED);

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
    /* 1. Flush D-Cache (Cortex-M7) changes to physical RAM */
    SCB_CleanDCache_by_Addr((uint32_t*)spi_tx_buffer, SPI_TX_SIZE);

    /* 2. Clear interrupt flags for stream 1 from previous transfer.
       If we skip this, the assignment DMA_Tx->CR |= DMA_SxCR_EN in the SPI driver
       will be silently ignored by the DMA controller!
       LIFCR - Lower Interrupt Flag Clear Register (for streams 0-3).
       CTCIF1 = Transfer Complete, CTEIF1 = Transfer Error. */
    DMA1->LIFCR = DMA_LIFCR_CTCIF1 | DMA_LIFCR_CTEIF1;

    /* 3. Delegation to SPI driver - Fire and forget */
    SPI_Transmit_DMA(SPI4, DMA1_Stream1, spi_tx_buffer, SPI_TX_SIZE);
}

/* Animation state variables */
static LED_Effect_t current_effect = LED_EFFECT_OFF;
static uint32_t effect_step = 0;

void LED_SetEffect(LED_Effect_t effect)
{
    if (current_effect != effect)
    {
        current_effect = effect;
        effect_step = 0; /* Reset animation step when effect changes */
    }
}

void LED_Process(void)
{
    switch (current_effect)
    {

        case LED_EFFECT_OFF:
        {
            if (effect_step == 0)
            {
                for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
                LED_Update();
                effect_step++; /* Prevent re-sending empty frames after DMA */
            }
            break;
        }

        case LED_EFFECT_SOLID_WHITE:
        {
            if (effect_step == 0)
            {
                for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 100, 100, 100);
                LED_Update();
                effect_step++;
            }
            break;
        }

        case LED_EFFECT_SPINNING_CYAN:
        {
            /* Speed: 1 full rotation in 400ms (100 steps at 100Hz).
               Divider 10 makes LED change every 100ms. */
            uint8_t head = (effect_step / 10) & 0x03; 
            uint8_t tail = (head - 1) & 0x03;

            for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
            LED_SetColor(head, 0, 200, 255);
            LED_SetColor(tail, 0, 20, 30);
            
            LED_Update();
            effect_step++;
            break;
        }

        case LED_EFFECT_STROBE_WARNING:
        {
            /* 100 Hz -> 100 steps is 1 second.
               Flashes last 30ms (3 steps) for better visibility. */
            uint32_t cycle = effect_step % 100;
            
            if (cycle == 0 || cycle == 2 || cycle == 10 || cycle == 12) 
            {
                LED_SetColor(LED_IDX_FRONT, 255, 255, 255);
                LED_SetColor(LED_IDX_BACK,  255, 255, 255);
                LED_SetColor(LED_IDX_LEFT,  255, 0,   0);
                LED_SetColor(LED_IDX_RIGHT, 255, 0,   0);
            }
            else 
            {
                for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
            }
            
            LED_Update();
            effect_step++;
            break;
        }

        case LED_EFFECT_PULSE_PINK:
        {
            /* 4 times slower than original (8 second cycle).
               At 100 Hz this gives 800 steps (400 for rise, 400 for fall). */
            uint32_t cycle = effect_step % 800;
            uint8_t brightness = 0;

            if (cycle < 400) 
                brightness = (cycle * 240) / 400;        /* Rise 0 -> 240 */
            else 
                brightness = ((799 - cycle) * 240) / 400; /* Fall 240 -> 0 */

            uint8_t final_r = brightness;
            uint8_t final_b = (brightness >> 1) + (brightness >> 2); 

            for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, final_r, 0, final_b);
            
            LED_Update();
            effect_step++;
            break;
        }

        case LED_EFFECT_CIRCLING_WAVE:
        {
            /* Smooth circling wave at 100 Hz.
               Full rotation in 2 seconds (200 steps). Offset between LEDs is 50 steps. */
            uint32_t step = effect_step % 200;

            for (uint8_t i = 0; i < LED_COUNT; i++) 
            {
                /* Phase calculation: (step + max_steps - (index * offset)) % max_steps */
                uint32_t phase = (step + 200 - (i * 50)) % 200; 
                uint8_t brightness = 0;

                /* Narrow bright wave peak (50 steps rise, 50 fall, 100 silent) */
                if (phase < 50) 
                    brightness = phase * 5;         /* 0 -> 250 */
                else if (phase < 100)
                    brightness = (99 - phase) * 5;  /* 250 -> 0 */
                else
                    brightness = 0;

                uint8_t final_g = 5 + (brightness >> 1);
                uint8_t final_b = 10 + brightness; /* Blue dominance for wave */

                LED_SetColor(i, 0, final_g, final_b);
            }
            
            LED_Update();
            effect_step++;
            break;
        }

        default:
            break;
    }
}