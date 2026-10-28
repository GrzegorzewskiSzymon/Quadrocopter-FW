/*
 * systick.h
 *
 * Created on: Mar 12, 2026
 * Author: Szymon Grzegorzewski
 */

#pragma once

#include <stdint.h>

/**
 * @brief  Initializes the SysTick timer to generate an interrupt every 1 millisecond.
 * @note   Assumes the core already runs at SYSCLK_HZ (call after RCC_Init()).
 */
void SysTick_Init(void);

/**
 * @brief  Provides a blocking delay in milliseconds based on the global counter.
 * @param  ms: Number of milliseconds to delay.
 */
void Delay_ms(uint32_t ms);

extern volatile uint32_t sys_tick_ms; /* Global system time counter, incremented in SysTick_Handler */