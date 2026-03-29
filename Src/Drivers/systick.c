/*
 * systick.c
 *
 *  Created on: Mar 12, 2026
 *      Author: Szymon Grzegorzewski
 */


#include "systick.h"
#include "stm32h723xx.h"

/* Global system time counter */
static volatile uint32_t sys_tick_ms = 0;

void SysTick_Init(void)
{
    /* 1 ms at 550 MHz core clock equals 550,000 clock cycles. */
    SysTick->LOAD = (550000UL - 1UL);
    SysTick->VAL = 0UL;

    /* Configuration:
       CLKSOURCE = 1 (Core clock, 550 MHz)
       TICKINT   = 1 (Enable interrupt for the Scheduler base time)
       ENABLE    = 1 (Enable the counter) */
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | 
                    SysTick_CTRL_TICKINT_Msk   | 
                    SysTick_CTRL_ENABLE_Msk;

    /* Set SysTick interrupt priority to the lowest level.
       Hard Real-Time tasks (DMA, EXTI) must preempt SysTick. */
    NVIC_SetPriority(SysTick_IRQn, (1UL << __NVIC_PRIO_BITS) - 1UL);
}

/* Standard CMSIS handler name for SysTick IRQ */
void SysTick_Handler(void)
{
    sys_tick_ms++;
}

uint32_t SysTick_GetMs(void)
{
    /* A 32-bit read on a 32-bit ARMv7-E architecture is intrinsically atomic.
       No need to disable interrupts here. */
    return sys_tick_ms;
}

void Delay_ms(uint32_t ms)
{
    uint32_t start_time = SysTick_GetMs();
    
    /* U2 arithmetic handles a single overflow perfectly */
    while ((SysTick_GetMs() - start_time) < ms)
    {
        /* Optional: Execute WFI (Wait For Interrupt) to save power 
           while blocking, instead of burning CPU cycles. */
        __WFI(); 
    }
}