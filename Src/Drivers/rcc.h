/*
 * rcc.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include <stdint.h>
#include "stm32h723xx.h"
#include "core_cm7.h"

/**
 * @brief  Initializes System Clock to 550 MHz using HSI TODO: Change to 32 MHz HSE.
 * @note   This function configures VOS0, Flash Wait States, and PLL1.
 * Must be called early in main() before any other peripheral initialization.
 */
void RCC_Init(void);
