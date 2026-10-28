/*
 * fsm.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "fsm.h"
#include "icm45686.h"
#include "nrf24l01.h"

/* TODO: Include RC/Receiver headers to check arming conditions */

static fsm_state_t current_state = FSM_STATE_INIT;

void FSM_Init(void) {
    current_state = FSM_STATE_DISARMED;
}

void FSM_EnterFailsafe(void) {
    current_state = FSM_STATE_FAILSAFE;
}

void FSM_Update(void) {
    /* Without attitude data or the operator link no state is safe;
       failsafe is latched until recovery is designed */
    if (current_state != FSM_STATE_FAILSAFE &&
        (!ICM45686_IsHealthy() || (NRF24_GetState() == NRF_STATE_FAULT))) {
        current_state = FSM_STATE_FAILSAFE;
    }

    /* State machine transition logic */
    switch (current_state) {
        case FSM_STATE_INIT:
            current_state = FSM_STATE_DEBUG; //TODO: Change to DISARMED after testing
            break;

        case FSM_STATE_DISARMED:
            /* TODO: if (RC_Arm_Condition_Met) current_state = FSM_STATE_ARMED; */
            break;

        case FSM_STATE_ARMED:
            /* TODO: if (RC_Disarm_Condition_Met) current_state = FSM_STATE_DISARMED; */
            /* TODO: if (Critical_Error_Detected) current_state = FSM_STATE_FAILSAFE; */
            break;

        case FSM_STATE_FAILSAFE:
            /* TODO: Handle recovery or lock down */
            break;
        
        case FSM_STATE_DEBUG:

            break;

        default:
            current_state = FSM_STATE_DISARMED;
            break;
    }
}

fsm_state_t FSM_GetState(void) {
    return current_state;
}
