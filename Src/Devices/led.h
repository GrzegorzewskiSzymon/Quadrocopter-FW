/*
 * led.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include <stdint.h>

#define LED_COUNT 4U

/* Physical arrangement of LEDs on drone arms */
#define LED_IDX_LEFT   0U
#define LED_IDX_BACK   1U
#define LED_IDX_RIGHT  2U
#define LED_IDX_FRONT  3U

/* Available hardware visual effects */
typedef enum {
    LED_EFFECT_OFF,
    LED_EFFECT_SOLID,         
    LED_EFFECT_SPINNING,       
    LED_EFFECT_STROBE,         
    LED_EFFECT_PULSE,          
    LED_EFFECT_CIRCLING_WAVE,   
    LED_EFFECT_ATTITUDE
} LED_Effect_t;

/* Dependency Injection structure */
typedef struct {
    /* Callback triggering DMA transfer at hardware layer */
    void (*TransmitCb)(uint8_t *tx_buffer, uint32_t size);
} LED_HwConfig_t;


void LED_Init(const LED_HwConfig_t *hw_config);
void LED_SetColor(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b);
void LED_Update(void);

void LED_SetEffect(LED_Effect_t effect, uint8_t r, uint8_t g, uint8_t b);

/* Setter for flight data (breaks hard dependency from control_loop.h) */
void LED_SetAttitudeData(float roll, float pitch);
void LED_Process(void);