/*
 * scheduler.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "scheduler.h"
#include "tasks.h"
#include "systick.h"


/* Task list - statically configured */
static task_t task_list[] = {
    {Task_FSM_Update,  2,   0},  /* 500 Hz */
    {Task_Baro,        10,  0},  /* 100 Hz */
    {Task_Telemetry,   20,  0},  /* 50 Hz  */
    {Task_LED,         10,  0}   /* 100 Hz  */
};

#define TASK_COUNT (sizeof(task_list) / sizeof(task_list[0]))

void Scheduler_Init(void) {
    uint32_t current_time = sys_tick_ms;
    
    /* Initialize start time for all tasks to prevent them from triggering simultaneously at boot */
    for (uint8_t i = 0; i < TASK_COUNT; i++) {
        task_list[i].last_run_ms = current_time;
    }
}

void Scheduler_Run(void) {
    uint32_t current_time = sys_tick_ms; 

    for (uint8_t i = 0; i < TASK_COUNT; i++) {
        /* U2 arithmetic automatically handles a single overflow */
        if ((current_time - task_list[i].last_run_ms) >= task_list[i].interval_ms) {
            task_list[i].func();
            task_list[i].last_run_ms = current_time;
        }
    }
}
