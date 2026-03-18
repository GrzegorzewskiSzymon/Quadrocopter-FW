/*
 * gpio.h
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

#include "stm32h723xx.h"
#include <stdint.h>

/* ========================================================================= */
/* GPIO CONFIGURATION DEFINES                                                */
/* ========================================================================= */

/* --- GPIO MODES (MODER) --- */
#define GPIO_MODE_INPUT         0x00U
#define GPIO_MODE_OUTPUT        0x01U
#define GPIO_MODE_AF            0x02U
#define GPIO_MODE_ANALOG        0x03U

/* --- GPIO OUTPUT TYPES (OTYPER) --- */
#define GPIO_OTYPE_PP           0x00U   /* Push-Pull */
#define GPIO_OTYPE_OD           0x01U   /* Open-Drain */

/* --- GPIO SPEED (OSPEEDR) --- */
#define GPIO_SPEED_LOW          0x00U
#define GPIO_SPEED_MED          0x01U
#define GPIO_SPEED_HIGH         0x02U
#define GPIO_SPEED_VHIGH        0x03U

/* --- GPIO PULL-UP/PULL-DOWN (PUPDR) --- */
#define GPIO_PUPD_NONE          0x00U
#define GPIO_PUPD_PU            0x01U
#define GPIO_PUPD_PD            0x02U

/* ========================================================================= */
/* PREPROCESSOR TUPLE EXTRACTION MACROS                                      */
/* ========================================================================= */

/* Internal macros for tuple expansion */
#define _GET_PORT(port, pin)    port
#define _GET_PIN(port, pin)     pin

/* Public variadic macros to extract port and pin from a defined tuple.
 * Using ... (__VA_ARGS__) prevents preprocessor token counting errors
 * when the macro expands to a comma-separated tuple. */
#define PORT(...)         _GET_PORT(__VA_ARGS__)
#define PIN(...)          _GET_PIN(__VA_ARGS__)

/* ========================================================================= */
/* INLINE FAST I/O OPERATIONS (ZERO-LATENCY)                                 */
/* ========================================================================= */

/**
 * @brief Atomically sets a GPIO pin high (1 cycle).
 */
static inline void GPIO_SetPin(GPIO_TypeDef *port, uint32_t pin_num)
{
    port->BSRR = (1U << pin_num);
}

/**
 * @brief Atomically resets a GPIO pin low (1 cycle).
 */
static inline void GPIO_ResetPin(GPIO_TypeDef *port, uint32_t pin_num)
{
    port->BSRR = (1U << (pin_num + 16U));
}

/**
 * @brief Toggles a GPIO pin state.
 */
static inline void GPIO_TogglePin(GPIO_TypeDef *port, uint32_t pin_num)
{
    port->ODR ^= (1U << pin_num);
}

/**
 * @brief Reads the input state of a GPIO pin.
 */
static inline uint8_t GPIO_ReadPin(GPIO_TypeDef *port, uint32_t pin_num)
{
    return ((port->IDR & (1U << pin_num)) != 0U) ? 1U : 0U;
}

/* ========================================================================= */
/* TUPLE WRAPPER MACROS FOR INLINE FUNCTIONS                                 */
/* ========================================================================= */

#define GPIO_SET(pin_macro)     GPIO_SetPin(pin_macro)
#define GPIO_RESET(pin_macro)   GPIO_ResetPin(pin_macro)
#define GPIO_TOGGLE(pin_macro)  GPIO_TogglePin(pin_macro)
#define GPIO_READ(pin_macro)    GPIO_ReadPin(pin_macro)

/* ========================================================================= */
/* INITIALIZATION PROTOTYPES & WRAPPERS                                      */
/* ========================================================================= */

void GPIO_InitClocks(void);
void GPIO_InitPin(GPIO_TypeDef *port, uint32_t pin_num, uint8_t mode, uint8_t otype, uint8_t ospeed, uint8_t pupd);
void GPIO_InitAF(GPIO_TypeDef *port, uint32_t pin_num, uint8_t af_num);

/* Wrappers for clean initialization using tuple macros */
#define GPIO_INIT(pin_macro, mode, otype, ospeed, pupd) \
    GPIO_InitPin(PORT(pin_macro), PIN(pin_macro), mode, otype, ospeed, pupd)

#define GPIO_INIT_AF(pin_macro, af_num) \
    GPIO_InitAF(PORT(pin_macro), PIN(pin_macro), af_num)
