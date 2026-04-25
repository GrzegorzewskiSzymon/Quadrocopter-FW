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
    LED_EFFECT_SOLID_WHITE,
    LED_EFFECT_SPINNING_CYAN,
    LED_EFFECT_STROBE_WARNING,
    LED_EFFECT_PULSE_BLUE,
    LED_EFFECT_PULSE_PINK,
    LED_EFFECT_CIRCLING_WAVE,
} LED_Effect_t;

void LED_Init(void);
void LED_SetColor(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b);
void LED_Update(void);
void LED_SetEffect(LED_Effect_t effect);
void LED_Process(void);

