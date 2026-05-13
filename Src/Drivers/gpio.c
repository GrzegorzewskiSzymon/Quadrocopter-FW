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

void GPIO_InitClocks(void)
{
    /* Enable clocks for all available GPIO ports on STM32H723 */
    RCC->AHB4ENR |= RCC_AHB4ENR_GPIOAEN | RCC_AHB4ENR_GPIOBEN |
                    RCC_AHB4ENR_GPIOCEN | RCC_AHB4ENR_GPIODEN |
                    RCC_AHB4ENR_GPIOEEN | RCC_AHB4ENR_GPIOFEN |
                    RCC_AHB4ENR_GPIOGEN | RCC_AHB4ENR_GPIOHEN;

    /* Dummy read to ensure clock propagation before returning */
    (void)RCC->AHB4ENR;
}

void NRF24_HW_Delay_us(uint32_t us)
{
    /* Pure in-line blocking delay. For 550 MHz: ~183 loop instructions / us */
    volatile uint32_t count = us * (550U / 3U);
    while (count--) { __NOP(); }
}

/* Configuration of EXTI for PD1 (RF_IRQ) */
void GPIO_NRF_EXTI_Init(void)
{
    /* Enable SYSCFG clock required for EXTI routing */
    RCC->APB4ENR |= RCC_APB4ENR_SYSCFGEN;
    (void)RCC->APB4ENR;

    /* Mapping EXTI1 line to Port D (PD1) - Register EXTICR1(0) */
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI1_Msk) | SYSCFG_EXTICR1_EXTI1_PD;

    /* Unmask interrupt on EXTI1 line */
    EXTI->IMR1 |= EXTI_IMR1_IM1;

    /* Configure falling edge trigger (NRF pulls line to GND) */
    EXTI->FTSR1 |= EXTI_FTSR1_TR1;

    /* Set priority in NVIC (Lower than DMA, higher than Scheduler) */
    NVIC_SetPriority(EXTI1_IRQn, 3);
    NVIC_EnableIRQ(EXTI1_IRQn);
}

/* Global EXTI1 interrupt handler */
void EXTI1_IRQHandler(void)
{
    /* Check and clear hardware interrupt flag from line 1 */
    if (EXTI->PR1 & EXTI_PR1_PR1)
    {
        EXTI->PR1 = EXTI_PR1_PR1; 

        /* Pass action to device layer */
        extern void NRF24_EXTI_Callback(void);
        NRF24_EXTI_Callback();
    }
}