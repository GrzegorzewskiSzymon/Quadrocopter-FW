/*
 * control_loop.h
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#pragma once

#include "icm45686.h"

void ControlLoop_Init(void);
void ControlLoop_Execute(ICM45686_Data_t *imu_data);
