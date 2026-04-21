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

#if !defined(__SOFT_FP__) && defined(__ARM_FP)
  #warning "FPU is not initialized, but the project is compiling for an FPU. Please initialize the FPU before use."
#endif



int main(void)
{
    /* 1. Hardware abstraction bring-up */
    MCU_Init();

    /* 2. Devices bring-up */
    // LED_Init();
    ICM45686_Init();
    
    for(;;)
    {

        // LED_SetColor(0, 255, 0, 0);
        // LED_SetColor(1, 255, 0, 0);
        // LED_SetColor(2, 255, 0, 0);
        // LED_SetColor(3, 255, 0, 0);
        // LED_Update();
        // Delay_ms(5);
        
        // LED_SetColor(0, 0, 0, 0);
        // LED_SetColor(1, 0, 0, 0);
        // LED_SetColor(2, 0, 0, 0);
        // LED_SetColor(3, 0, 0, 0);
        // LED_Update();
        // Delay_ms(500);


    }
}
