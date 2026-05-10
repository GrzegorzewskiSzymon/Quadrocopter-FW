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
#include "nrf24l01.h"


int main(void)
{
    /* 1. Hardware abstraction bring-up */
    MCU_Init();

    Scheduler_Init();

    /* 2. Devices bring-up */
    LED_Init();
    ICM45686_Init();
    NRF24_Test_Init();
    NRF24_Test_Run();
    for(;;)
    {
        Scheduler_Run();



    }
}
