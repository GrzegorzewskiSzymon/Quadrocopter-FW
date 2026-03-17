/*
 * mcu_init.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

/**
 * @brief  Master initialization routine for the microcontroller.
 * @note   Brings up clocks, power domains, basic GPIO, and system tick.
 */
void MCU_Init(void);
