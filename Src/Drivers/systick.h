/*
 * systick.h
 *
 *  Created on: Mar 12, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include <stdint.h>

/**
 * @brief  Initializes the SysTick timer to generate a flag every 1 millisecond.
 * @note   This function assumes the core clock (SYSCLK) is exactly 550 MHz.
 */
void SysTick_Init(void);

/**
 * @brief  Provides a blocking (busy-wait) delay in milliseconds.
 * @param  ms: Number of milliseconds to delay.
 */
void Delay_ms(uint32_t ms);
