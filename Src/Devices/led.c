/*
 * led.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "led.h"
#include "../Drivers/gpio.h"
#include "../Config/board_config.h"

void LED_Init(void)
{
	/* Initialize all LED pins as Push-Pull Output, Low Speed, Pull-Down */
	GPIO_InitPin(PORT(LED_RED),    PIN(LED_RED),    GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_YELLOW), PIN(LED_YELLOW), GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_GREEN),  PIN(LED_GREEN),  GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);
	GPIO_InitPin(PORT(LED_BLUE),   PIN(LED_BLUE),   GPIO_MODE_OUTPUT, GPIO_OTYPE_PP, GPIO_SPEED_LOW, GPIO_PUPD_PD);

	/* Ensure all LEDs are turned off at startup */
	GPIO_RESET(LED_RED);
	GPIO_RESET(LED_YELLOW);
	GPIO_RESET(LED_GREEN);
	GPIO_RESET(LED_BLUE);
}

