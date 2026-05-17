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
#include "drivers_config.h"


int main(void)
{
    /* 1. Hardware abstraction bring-up */
    MCU_Init();

    Scheduler_Init();

    /* 2. Devices bring-up */
    BOARD_LED_Init();
    BOARD_IMU_Init();
    // NRF24_Test_Init();
    BOARD_Radio_Init();
    NRF24_Payload_t test_payload = {
        .aileron = 1500U,
        .elevator = 1500U,
        .throttle = 1000U,
        .rudder = 1500U,
        .switches_a = 0x01U, /* Armed */
        .switches_b = 0x00U, /* Beeper off, OSD off */
        .telemetry_req = 0x00U /* No telemetry requested */
    };
    NRF24_Transmit_IT(&test_payload);
    for(;;)
    {
        Scheduler_Run();



    }
}
