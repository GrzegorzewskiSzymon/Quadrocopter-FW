/*
 * fsm.h
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#pragma once
#include <stdint.h>

typedef enum {
    FSM_STATE_INIT = 0,
    FSM_STATE_DISARMED,
    FSM_STATE_ARMED,
    FSM_STATE_FAILSAFE
} fsm_state_t;

void FSM_Init(void);
void FSM_Update(void);
fsm_state_t FSM_GetState(void);
