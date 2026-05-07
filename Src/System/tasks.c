/*
 * tasks.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "tasks.h"
#include "led.h"


void Task_FSM_Update(void) {
    /* TODO: Call FSM_Update() */
}

void Task_Baro(void) {
    /* TODO: Trigger I2C4 read for BMP390 */
}

void Task_Telemetry(void) {
    /* TODO: Check FSM_GetState(), format payload, push to NRF24 SPI buffer */
}

void Task_LED(void)
{
    /* TODO: 
     * fsm_state_t state = FSM_GetState();
     * switch(state) { case FSM_IDLE: ... case FSM_ARMED: ... } 
     */
    LED_SetEffect(LED_EFFECT_ATTITUDE, 50, 0, 0);
    LED_Process();
}
