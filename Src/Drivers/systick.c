/*
 * systick.c
 *
 *  Created on: Mar 12, 2026
 *      Author: Szymon Grzegorzewski
 */


#include "systick.h"
#include "stm32h723xx.h"

void SysTick_Init(void)
{
    /* 1 ms at 550 MHz core clock equals 550,000 clock cycles.
       This value fits within the 24-bit LOAD register (max 16,777,215). */
    SysTick->LOAD = (550000UL - 1UL);

    /* Clear the current value register */
    SysTick->VAL = 0UL;

    /* Configuration:
       CLKSOURCE = 1 (Core clock, 550 MHz)
       TICKINT   = 0 (Disable interrupt, we use polling for determinism in delay)
       ENABLE    = 1 (Enable the counter) */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;
}

void Delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        /* Wait until the COUNTFLAG (bit 16) is set by hardware.
           Reading the CTRL register automatically clears this flag. */
        while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0)
        {
            /* Busy-wait at full 550 MHz speed */
        }
    }
}
