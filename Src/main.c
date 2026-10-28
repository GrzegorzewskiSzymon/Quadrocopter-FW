/*
 * main.c
 *
 * Created on: Feb 28, 2026
 * Author: Szymon Grzegorzewski
 * Description: Main entry point for the custom R&D flight controller firmware.
 */

#include "drivers_config.h"
#include "stm32h723xx.h"
#include "mcu_init.h"
#include "systick.h"
#include "led.h"
#include "icm45686.h"
#include "scheduler.h"
#include "nrf24l01.h"
#include "fsm.h"


int main(void)
{
    /* 1. Hardware abstraction bring-up */
    if (MCU_Init() != STATUS_OK)
    {
        /* Without the expected clock tree no timing-dependent code is valid. Motors are not
           initialised yet, so halting is the safe state (needs a power cycle until IWDG exists) */
        __disable_irq();
        for (;;) { }
    }

    Scheduler_Init();

    /* 2. Devices bring-up */
    BOARD_LED_Init();
    if (BOARD_IMU_Init() != STATUS_OK)
    {
        /* Keep running so the fault can be signalled and reported over the radio */
        FSM_EnterFailsafe();
    }
    if (BOARD_Radio_Init() != STATUS_OK)
    {
        /* The radio is the only link to the operator (future RC input) */
        FSM_EnterFailsafe();
    }


    for(;;)
    {
        Scheduler_Run();
    }
}
