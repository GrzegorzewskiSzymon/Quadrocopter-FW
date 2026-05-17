/*
 * led.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "led.h"
#include "control_loop.h"
#include "stm32h723xx.h"
#include "stddef.h"

#define BITS_PER_LED          24U
#define SPI_TX_SIZE           128U 
#define WS2812_0_CODE         0xC0U 
#define WS2812_1_CODE         0xFCU 
#define WS2812_PREAMBLE_BYTES 16U

/* CRITICAL: 32-byte alignment for D-Cache line boundary. Placed in default RAM for DMA1 access. */
__attribute__((aligned(32))) static uint8_t spi_tx_buffer[SPI_TX_SIZE];

/* Hardware abstraction callback */
static void (*led_transmit_cb)(uint8_t *tx_buffer, uint32_t size) = NULL;

/* Attitude data decoupled from Flight Loop */
static float current_roll = 0.0f;
static float current_pitch = 0.0f;

static LED_Effect_t current_effect = LED_EFFECT_OFF;
static uint32_t effect_step = 0;

static uint8_t active_r = 0;
static uint8_t active_g = 0;
static uint8_t active_b = 0;

/* --- PUBLIC API --- */

void LED_Init(const LED_HwConfig_t *hw_config)
{
    /* Dependency Injection */
    if (hw_config != NULL) {
        led_transmit_cb = hw_config->TransmitCb;
    }

    /* Buffer zeroing (Reset Latch padding) */
    for (uint32_t i = 0; i < SPI_TX_SIZE; i++)
    {
        spi_tx_buffer[i] = 0x00U;
    }
}

void LED_SetColor(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b)
{
    if (led_index >= LED_COUNT) return;

    uint32_t color = ((uint32_t)g << 16) | ((uint32_t)r << 8) | (uint32_t)b;
    
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

    /* 2. Delegation to Board Config layer to fire DMA */
    if (led_transmit_cb != NULL)
    {
        led_transmit_cb(spi_tx_buffer, SPI_TX_SIZE);
    }
}

void LED_SetAttitudeData(float roll, float pitch)
{
    current_roll = roll;
    current_pitch = pitch;
}

/* --- EFFECTS IMPLEMENTATION --- */

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
    LED_SetColor(tail, r >> 3, g >> 3, b >> 3); 
    
    LED_Update();
    effect_step++;
}

static void Effect_Strobe(uint8_t r, uint8_t g, uint8_t b) {
    uint32_t cycle = effect_step % 100;
    
    if (cycle == 0 || cycle == 2 || cycle == 10 || cycle == 12) {
        LED_SetColor(LED_IDX_FRONT, 255, 255, 255); 
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
    uint32_t scale = (cycle < 400) ? cycle : (799 - cycle);

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
        uint32_t scale = (phase < 50) ? (phase * 5) : (phase < 100 ? (99 - phase) * 5 : 0);
        
        uint8_t cur_r = (r / 20) + ((r * scale) / 250);
        uint8_t cur_g = (g / 20) + ((g * scale) / 250);
        uint8_t cur_b = (b / 20) + ((b * scale) / 250);

        LED_SetColor(i, cur_r, cur_g, cur_b);
    }
    LED_Update();
    effect_step++;
}

static void Effect_Attitude(uint8_t r, uint8_t g, uint8_t b) {
    /* Using local variables updated via LED_SetAttitudeData() */
    LED_SetAttitudeData(roll,pitch); /* TODO: Change to actual roll and pitch values */
    float r_val = current_roll * 5.0f;
    float p_val = current_pitch * 5.0f;

    uint32_t imu_r = (r_val > 0.0f) ? (uint32_t)r_val : 0U;
    uint32_t imu_l = (r_val < 0.0f) ? (uint32_t)(-r_val) : 0U;
    uint32_t imu_f = (p_val < 0.0f) ? (uint32_t)(-p_val) : 0U; 
    uint32_t imu_b = (p_val > 0.0f) ? (uint32_t)p_val : 0U;     

    if (imu_r > 255) imu_r = 255;
    if (imu_l > 255) imu_l = 255;
    if (imu_f > 255) imu_f = 255;
    if (imu_b > 255) imu_b = 255;

    const uint8_t G_SUBTLE_MAX = 5; 

    LED_SetColor(LED_IDX_LEFT,  (r * imu_r) / 255, (G_SUBTLE_MAX * (255 - imu_r)) / 255, (b * imu_r) / 255); 
    LED_SetColor(LED_IDX_RIGHT, (r * imu_l) / 255, (G_SUBTLE_MAX * (255 - imu_l)) / 255, (b * imu_l) / 255); 
    LED_SetColor(LED_IDX_BACK,  (r * imu_b) / 255, (G_SUBTLE_MAX * (255 - imu_b)) / 255, (b * imu_b) / 255);
    LED_SetColor(LED_IDX_FRONT, (r * imu_f) / 255, (G_SUBTLE_MAX * (255 - imu_f)) / 255, (b * imu_f) / 255);

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
        effect_handlers[current_effect](active_r, active_g, active_b);
    }
}