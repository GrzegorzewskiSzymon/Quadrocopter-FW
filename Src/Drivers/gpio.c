/*
 * gpio.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */


#include "gpio.h"

/**
 * @brief Initializes a GPIO pin with specified parameters via direct register access.
 * * @param port   GPIO Port (e.g., GPIOA, GPIOB)
 * @param pin_num Pin number (0 to 15)
 * @param mode   GPIO_MODE_...
 * @param otype  GPIO_OTYPE_...
 * @param ospeed GPIO_SPEED_...
 * @param pupd   GPIO_PUPD_...
 */
void GPIO_InitPin(GPIO_TypeDef *port, uint32_t pin_num, uint8_t mode, uint8_t otype, uint8_t ospeed, uint8_t pupd)
{
    /* Calculate shift values */
    uint32_t shift_2bit = pin_num * 2U;

    /* 1. Clear configuration bits */
    port->MODER   &= ~(0x3U << shift_2bit);
    port->OTYPER  &= ~(0x1U << pin_num);
    port->OSPEEDR &= ~(0x3U << shift_2bit);
    port->PUPDR   &= ~(0x3U << shift_2bit);

    /* 2. Set new configuration bits */
    port->MODER   |= ((uint32_t)mode   << shift_2bit);
    port->OTYPER  |= ((uint32_t)otype  << pin_num);
    port->OSPEEDR |= ((uint32_t)ospeed << shift_2bit);
    port->PUPDR   |= ((uint32_t)pupd   << shift_2bit);
}

/**
 * @brief Configures the Alternate Function (AF) for a specific GPIO pin.
 * @param port   GPIO Port (e.g., GPIOA, GPIOB)
 * @param pin_num Pin number (0 to 15)
 * @param af_num Alternate function number (0 to 15)
 */
void GPIO_InitAF(GPIO_TypeDef *port, uint32_t pin_num, uint8_t af_num)
{
    /* Determine if we need to use AFR[0] (pins 0-7) or AFR[1] (pins 8-15) */
    uint32_t afr_index = pin_num >> 3U;

    /* Calculate bit shift within the appropriate AFR register (4 bits per pin) */
    uint32_t afr_shift = (pin_num & 0x7U) * 4U;

    /* Clear and set Alternate Function bits */
    port->AFR[afr_index] &= ~(0xFU << afr_shift);
    port->AFR[afr_index] |= ((uint32_t)af_num << afr_shift);
}
