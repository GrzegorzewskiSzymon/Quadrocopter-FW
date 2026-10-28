/*
 * dwt.h
 *
 *  Created on: Oct 29, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h723xx.h"
#include "rcc.h"
#include <stdint.h>
#include <stdbool.h>

/* Core cycle counter used as the time base for bounded hardware polling.
   SysTick cannot serve here: it does not advance inside ISRs of higher priority
   and does not run yet during RCC_Init(). */

void DWT_Init(void);

static inline uint32_t DWT_GetCycles(void)
{
    return DWT->CYCCNT;
}

/* Budget is computed for SYSCLK_HZ; before the PLL switch the core runs slower,
   so the real timeout is longer but still bounded. Valid for timeouts < 7.8 s
   (CYCCNT wraps every 2^32 cycles at 550 MHz). */
static inline bool DWT_IsElapsedUs(uint32_t start_cycles, uint32_t timeout_us)
{
    return (DWT_GetCycles() - start_cycles) >= (timeout_us * (SYSCLK_HZ / 1000000UL));
}
