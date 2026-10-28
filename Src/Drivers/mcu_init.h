/*
 * mcu_init.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "status.h"

/**
 * @brief  Master initialization routine for the microcontroller.
 * @note   Brings up clocks, power domains, basic GPIO, and system tick.
 * @retval STATUS_ERR_TIMEOUT if the clock tree could not be brought up
 */
Status_t MCU_Init(void);
