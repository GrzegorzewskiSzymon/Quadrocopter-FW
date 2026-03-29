/*
 * scheduler.h
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#pragma once
#include <stdint.h>

typedef void (*task_func_t)(void);

typedef struct {
    task_func_t func;
    uint32_t interval_ms;
    uint32_t last_run_ms;
} task_t;

void Scheduler_Init(void);
void Scheduler_Run(void);
