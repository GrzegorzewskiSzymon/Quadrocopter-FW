/*
 * scheduler.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "scheduler.h"
#include "tasks.h"

/* TODO: Include system time source here (e.g., systick.h) */
/* extern volatile uint32_t sys_tick_ms; */

/* Task list - statically configured */
static task_t task_list[] = {
    {Task_FSM_Update,  2,   0},  /* 500 Hz */
    {Task_Baro,        10,  0},  /* 100 Hz */
    {Task_Telemetry,   20,  0},  /* 50 Hz  */
    {Task_LED,         50,  0}   /* 20 Hz  */
};

#define TASK_COUNT (sizeof(task_list) / sizeof(task_list[0]))

void Scheduler_Init(void) {
    /* TODO: Reset all last_run_ms to current system time if needed */
}

void Scheduler_Run(void) {
    /* TODO: Get atomic current time from SysTick */
    uint32_t current_time = 0; 

    for (uint8_t i = 0; i < TASK_COUNT; i++) {
        /* U2 arithmetic automatically handles a single overflow */
        if ((current_time - task_list[i].last_run_ms) >= task_list[i].interval_ms) {
            task_list[i].func();
            task_list[i].last_run_ms = current_time;
        }
    }
}
