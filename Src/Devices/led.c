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
#include "control_loop.h"

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


static LED_Effect_t current_effect = LED_EFFECT_OFF;
static uint32_t effect_step = 0;

/* We store the base color of the current effect */
static uint8_t active_r = 0;
static uint8_t active_g = 0;
static uint8_t active_b = 0;

static void Effect_Off(uint8_t r, uint8_t g, uint8_t b) {
    if (effect_step == 0) {
        for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
        LED_Update();
        effect_step++;
    }
}

static void Effect_Solid(uint8_t r, uint8_t g, uint8_t b) {
    if (effect_step == 0) {
        for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, r, g, b);
        LED_Update();
        effect_step++;
    }
}

static void Effect_Spinning(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t head = (effect_step / 10) & 0x03; 
    uint8_t tail = (head - 1) & 0x03;

    for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
    
    LED_SetColor(head, r, g, b);
    /* Tail has 12.5% brightness of the base color (bit shift >> 3 is division by 8) */
    LED_SetColor(tail, r >> 3, g >> 3, b >> 3); 
    
    LED_Update();
    effect_step++;
}

static void Effect_Strobe(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t cycle = effect_step % 100;
    
    if (cycle == 0 || cycle == 2 || cycle == 10 || cycle == 12) {
        /* Flashes use the color specified in parameters (e.g., red for warnings) */
        LED_SetColor(LED_IDX_FRONT, 255, 255, 255); /* Front is always white as orientation reference */
        LED_SetColor(LED_IDX_BACK,  255, 255, 255);
        LED_SetColor(LED_IDX_LEFT,  r, g, b);
        LED_SetColor(LED_IDX_RIGHT, r, g, b);
    } else {
        for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, 0, 0, 0);
    }
    
    LED_Update();
    effect_step++;
}

static void Effect_Pulse(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t cycle = effect_step % 800;
    
    /* Variable 'scale' from 0 to 400 */
    uint32_t scale = (cycle < 400) ? cycle : (799 - cycle);

    /* Scale each channel separately.
       Multiplication before division preserves fixed-point precision. */
    uint8_t cur_r = (r * scale) / 400;
    uint8_t cur_g = (g * scale) / 400;
    uint8_t cur_b = (b * scale) / 400;

    for (uint8_t i = 0; i < LED_COUNT; i++) LED_SetColor(i, cur_r, cur_g, cur_b);
    
    LED_Update();
    effect_step++;
}

static void Effect_CirclingWave(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t step = effect_step % 200;
    for (uint8_t i = 0; i < LED_COUNT; i++) {
        uint32_t phase = (step + 200 - (i * 50)) % 200; 
        
        /* Brightness scale: 0 to 250 */
        uint32_t scale = (phase < 50) ? (phase * 5) : (phase < 100 ? (99 - phase) * 5 : 0);
        
        /* Base glow is ~5% of the base color */
        uint8_t cur_r = (r / 20) + ((r * scale) / 250);
        uint8_t cur_g = (g / 20) + ((g * scale) / 250);
        uint8_t cur_b = (b / 20) + ((b * scale) / 250);

        LED_SetColor(i, cur_r, cur_g, cur_b);
    }
    LED_Update();
    effect_step++;
}

static void Effect_Attitude(uint8_t r, uint8_t g, uint8_t b) {
    /* 1. Calculate raw tilt intensities from IMU data */
    float r_val = roll * 5.0f;
    float p_val = pitch * 5.0f;

    uint32_t imu_r = (r_val > 0.0f) ? (uint32_t)r_val : 0U;
    uint32_t imu_l = (r_val < 0.0f) ? (uint32_t)(-r_val) : 0U;
    uint32_t imu_f = (p_val < 0.0f) ? (uint32_t)(-p_val) : 0U; /* Pitch < 0 = Nose Down in IMU */
    uint32_t imu_b = (p_val > 0.0f) ? (uint32_t)p_val : 0U;     /* Pitch > 0 = Tail Down in IMU */

    /* Clipping at 255 */
    if (imu_r > 255) imu_r = 255;
    if (imu_l > 255) imu_l = 255;
    if (imu_f > 255) imu_f = 255;
    if (imu_b > 255) imu_b = 255;

    /* 2. Intensity configuration */
    const uint8_t G_SUBTLE_MAX = 5; /* Very dim green when level */


    
    /* Physical LEFT LED */
    LED_SetColor(LED_IDX_LEFT,  (r * imu_r) / 255, (G_SUBTLE_MAX * (255 - imu_r)) / 255, (b * imu_r) / 255); 
    LED_SetColor(LED_IDX_RIGHT, (r * imu_l) / 255, (G_SUBTLE_MAX * (255 - imu_l)) / 255, (b * imu_l) / 255); 
    LED_SetColor(LED_IDX_BACK, (r * imu_b) / 255, (G_SUBTLE_MAX * (255 - imu_b)) / 255, (b * imu_b) / 255);
    LED_SetColor(LED_IDX_FRONT,  (r * imu_f) / 255, (G_SUBTLE_MAX * (255 - imu_f)) / 255, (b * imu_f) / 255);

    LED_Update();
    effect_step++;
}


typedef void (*LED_EffectHandler_t)(uint8_t r, uint8_t g, uint8_t b);

static const LED_EffectHandler_t effect_handlers[] = {
    [LED_EFFECT_OFF]           = Effect_Off,
    [LED_EFFECT_SOLID]         = Effect_Solid,
    [LED_EFFECT_SPINNING]      = Effect_Spinning,
    [LED_EFFECT_STROBE]        = Effect_Strobe,
    [LED_EFFECT_PULSE]         = Effect_Pulse,
    [LED_EFFECT_CIRCLING_WAVE] = Effect_CirclingWave,
    [LED_EFFECT_ATTITUDE]      = Effect_Attitude
};

#define EFFECT_HANDLERS_COUNT (sizeof(effect_handlers) / sizeof(effect_handlers[0]))


void LED_SetEffect(LED_Effect_t effect, uint8_t r, uint8_t g, uint8_t b) {
    if (current_effect != effect || active_r != r || active_g != g || active_b != b) {
        current_effect = effect;
        active_r = r;
        active_g = g;
        active_b = b;
        effect_step = 0;
    }
}

void LED_Process(void) {
    if (current_effect < EFFECT_HANDLERS_COUNT && effect_handlers[current_effect] != 0U) {
        /* Pass the stored color to the appropriate effect function */
        effect_handlers[current_effect](active_r, active_g, active_b);
    }
}