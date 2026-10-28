/*
 * tasks.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "tasks.h"
#include "led.h"
#include "nrf24l01.h"
#include "telemetry_queue.h"
#include "fsm.h"
#include "icm45686.h"


void Task_IMU_Watchdog(void) {
    ICM45686_CheckStall();
}

void Task_FSM_Update(void) {
    FSM_Update();
}

void Task_Baro(void) {
    /* TODO: Trigger I2C4 read for BMP390 */
}

void Task_Telemetry(void) {
    /* Kickstart the transmission chain if NRF is sleeping but queue is pending */
    if (NRF24_GetState() == NRF_STATE_IDLE) 
    {
        NRF24_Payload_t payload;
        if (TelemetryQueue_Pop(&payload))
        {
            /* Telemetry is best effort: a failed start is latched as NRF_STATE_FAULT,
               which the FSM escalates */
            (void)NRF24_Transmit_IT(&payload);
        }
    }
}

void Task_LED(void)
{
    /* TODO: 
     * fsm_state_t state = FSM_GetState();
     * switch(state) { case FSM_IDLE: ... case FSM_ARMED: ... } 
     */
    if (FSM_GetState() == FSM_STATE_FAILSAFE)
    {
        LED_SetEffect(LED_EFFECT_STROBE, 255, 0, 0);
    }
    else
    {
        LED_SetEffect(LED_EFFECT_ATTITUDE, 50, 0, 0);
    }
    LED_Process();
}
