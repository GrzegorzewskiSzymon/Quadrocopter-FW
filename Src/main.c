/*
 * main.c
 *
 * Created on: Feb 28, 2026
 * Author: Szymon Grzegorzewski
 * Description: Main entry point for the custom R&D flight controller firmware.
 */

#include "stm32h723xx.h"
#include "mcu_init.h"
#include "systick.h"
#include "led.h"
#include "icm45686.h"
#include "scheduler.h"


int main(void)
{
    /* 1. Hardware abstraction bring-up */
    MCU_Init();

    Scheduler_Init();

    /* 2. Devices bring-up */
    LED_Init();
    ICM45686_Init();

    for(;;)
    {
        Scheduler_Run();


    }
}
