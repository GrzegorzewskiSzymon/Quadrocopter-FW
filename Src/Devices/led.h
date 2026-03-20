/*
 * led.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include <stdint.h>

#define LED_COUNT 4U

void LED_Init(void);
void LED_SetColor(uint32_t led_index, uint8_t r, uint8_t g, uint8_t b);
void LED_Update(void);

