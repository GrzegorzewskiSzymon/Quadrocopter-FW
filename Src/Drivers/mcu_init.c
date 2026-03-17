/*
 * mcu_init.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */


#include "mcu_init.h"
#include "rcc.h"
#include "gpio.h"
#include "systick.h"

void MCU_Init(void)
{
    /* 1. Initialize core system clock (550 MHz), caches and power domains */
    RCC_Init();

    /* 2. Enable GPIO hardware clocks on AHB4 bus */
    GPIO_InitClocks();

    /* 3. Systick setup for 1ms base time */
    SysTick_Init();

    /* 4. (Placeholder) Initialize FPU context, DMA controllers, etc. */
}
