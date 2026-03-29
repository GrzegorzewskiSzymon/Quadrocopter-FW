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

static void DBGMCU_Init(void) 
{
    /* Enable debug in low-power modes for the D1 domain (Cortex-M7).
       Prevents SWD disconnect if the core enters WFI/WFE states. */
    DBGMCU->CR |= DBGMCU_CR_DBG_SLEEPD1 | 
                  DBGMCU_CR_DBG_STOPD1  | 
                  DBGMCU_CR_DBG_STANDBYD1;

    /* Freeze hardware watchdogs. 
       Prevents the core from a hard reset while halted on a breakpoint. */
    DBGMCU->APB3FZ1 |= DBGMCU_APB3FZ1_DBG_WWDG1;
    DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;

    /* Future note: Freeze timers used for ESCs (e.g., TIM2/TIM3) 
       to prevent motors from spinning while the debugger is halted.
       DBGMCU->APB1LFZ1 |= DBGMCU_APB1LFZ1_DBG_TIM2; 
    */
}

void MCU_Init(void)
{
    /* 1. Initialize core system clock (550 MHz), caches and power domains */
    RCC_Init();

    /* 2. Enable GPIO hardware clocks on AHB4 bus */
    GPIO_InitClocks();

    /* 3. Configure debug features to ensure a stable debugging environment */
    DBGMCU_Init();

    /* 4. Systick setup for 1ms base time */
    SysTick_Init();

    /* 5. (Placeholder) Initialize FPU context, DMA controllers, etc. */
}
