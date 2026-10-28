/*
 * drivers_config.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "status.h"

Status_t BOARD_Radio_Init(void);

Status_t BOARD_IMU_Init(void);

void BOARD_LED_Init(void);
