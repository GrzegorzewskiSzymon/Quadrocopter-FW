/*
 * icm45686.h
 *
 *  Created on: Mar 29, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h723xx.h"
#include <stdint.h>

#define ICM45686_REG_WHO_AM_I 0x72
#define ICM45686_SPI_READ_BIT 0x80

void ICM45686_Init(void);
uint8_t ICM45686_ReadWhoAmI(void);
