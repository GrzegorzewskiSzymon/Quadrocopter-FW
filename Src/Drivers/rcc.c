/*
 * rcc.c
 *
 *  Created on: Feb 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "rcc.h"

void RCC_Init(void)
{
    /* 1. ENABLE CORTEX-M7 CACHES (CRITICAL FOR 550 MHz PERFORMANCE) */
    SCB_EnableICache();
    SCB_EnableDCache();

    /* 2. POWER CONFIGURATION: LDO ENABLE */
    PWR->CR3 = (PWR->CR3 & ~PWR_CR3_LDOEN) | PWR_CR3_LDOEN;

    /* Wait until active voltage regulator is ready */
    while ((PWR->CSR1 & PWR_CSR1_ACTVOSRDY) == 0) {}

    /* 3. VOLTAGE SCALING (VOS0) FOR 550 MHz */
    /* In H723, VOS0 is set directly via PWR_D3CR VOS field. */
    PWR->D3CR = (PWR->D3CR & ~PWR_D3CR_VOS) | (3 << PWR_D3CR_VOS_Pos);

    /* Wait until voltage scaling is ready in VOS0 mode */
    while ((PWR->D3CR & PWR_D3CR_VOSRDY) == 0) {}

    /* 4. ENABLE INTERNAL OSCILLATOR (HSI 64 MHz) */
    /* Enable HSI and wait for it to stabilize */
    RCC->CR |= RCC_CR_HSION;
    while ((RCC->CR & RCC_CR_HSIRDY) == 0) {}

    /* 5. FLASH MEMORY CONFIGURATION (WAIT STATES) */
    /* VOS0 & AXI clock = 275 MHz requires 3 Wait States (Latency) and WRHIGHFREQ = 2 */
    FLASH->ACR = (FLASH->ACR & ~(FLASH_ACR_LATENCY | FLASH_ACR_WRHIGHFREQ)) |
                 (3 << FLASH_ACR_LATENCY_Pos) |
                 (2 << FLASH_ACR_WRHIGHFREQ_Pos);

    /* Verify if Flash Latency was successfully applied */
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != (3 << FLASH_ACR_LATENCY_Pos)) {}

    /* 6. PLL1 CONFIGURATION (USING HSI) */
    /* Select HSI (value 0) as PLL source, set DIVM1 = 32 (64 MHz / 32 = 2 MHz Ref Clock) */
    RCC->PLLCKSELR = (0 << RCC_PLLCKSELR_PLLSRC_Pos) |
                     (32 << RCC_PLLCKSELR_DIVM1_Pos);

    /* Configure PLL1 Range (2-4 MHz input -> Wide VCO), Enable Dividers */
    RCC->PLLCFGR = (2 << RCC_PLLCFGR_PLL1RGE_Pos) |
                   RCC_PLLCFGR_DIVP1EN | RCC_PLLCFGR_DIVQ1EN | RCC_PLLCFGR_DIVR1EN;

    /* VCO = Ref (2 MHz) * DIVN1. Target VCO = 550 MHz -> DIVN1 = 275. (Write 274)
     * SYSCLK = VCO / DIVP1. Target 550 MHz -> DIVP1 = 1. (Write 0)
     * Setting Q=2 and R=2 as defaults. */
    RCC->PLL1DIVR = (274 << RCC_PLL1DIVR_N1_Pos) |
                    (0   << RCC_PLL1DIVR_P1_Pos) |
                    (1   << RCC_PLL1DIVR_Q1_Pos) |
                    (1   << RCC_PLL1DIVR_R1_Pos);

    /* 7. ACTIVATE PLL1 */
    RCC->CR |= RCC_CR_PLL1ON;
    while ((RCC->CR & RCC_CR_PLL1RDY) == 0) {}

    /* 8. CONFIGURE BUS PRESCALERS */
    /* SYSCLK = 550 MHz, HCLK = 275 MHz, APB1/2/3/4 = 137.5 MHz */
    RCC->D1CFGR = (0 << RCC_D1CFGR_D1CPRE_Pos) |
                  (8 << RCC_D1CFGR_HPRE_Pos) |
                  (4 << RCC_D1CFGR_D1PPRE_Pos);

    RCC->D2CFGR = (4 << RCC_D2CFGR_D2PPRE1_Pos) |
                  (4 << RCC_D2CFGR_D2PPRE2_Pos);

    RCC->D3CFGR = (4 << RCC_D3CFGR_D3PPRE_Pos);

    /* 9. SWITCH SYSTEM CLOCK SOURCE TO PLL1 */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | (3 << RCC_CFGR_SW_Pos);

    /* Wait until system uses PLL1 as its clock source */
    while ((RCC->CFGR & RCC_CFGR_SWS) != (3 << RCC_CFGR_SWS_Pos)) {}
}
