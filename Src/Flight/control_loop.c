/*
 * control_loop.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "control_loop.h"
#include "stm32h723xx.h"

/* TODO: Include FSM, IMU, ESC (Timer CCR) headers */

void ControlLoop_Init(void) {
    /* TODO: Initialize PID controllers, EKF matrices */
}

void ControlLoop_Execute(void) {
    /* * CRITICAL EXECUTION PATH - ZERO BLOCKING CALLS
     * Triggered from hardware interrupt (e.g., DMA_TC for SPI)
     */

    /* 1. Read data from DMA IMU buffer */
    /* 2. Apply filters (LPF, Notch) */
    /* 3. State estimation (EKF) */

    /* 4. Check drone state (Context) */
    /* TODO: fsm_state_t state = FSM_GetState(); */

    /* TODO: 
     * if (state == FSM_STATE_ARMED) {
     * - Calculate PID
     * - Motor mixer
     * - Write to TIMx->CCR1..4 (Update PWM/DSHOT ESC)
     * } else {
     * - Reset PID integrals (Anti-windup)
     * - Write 0 to TIMx->CCR1..4 (Disable motors)
     * }
     */
}

/* Standard CMSIS handler name for EXTI Line 4 */
void EXTI4_IRQHandler(void)
{
    /* Quick flag check and clear (Zero-Overhead) */
    if (EXTI->PR1 & EXTI_PR1_PR4)
    {
        EXTI->PR1 = EXTI_PR1_PR4; /* Writing 1 clears bit in rc_w1 registers (Read/Clear Write 1) */
        
        /* Here you insert a software flag, RTOS semaphore or call DMA for SPI read */
    }
}
