/*
 * control_loop.c
 *
 * Created on: Mar 29, 2026
 * Author: Szymon Grzegorzewski
 */

#include "control_loop.h"
#include "stm32h723xx.h"
#include <math.h>

/* TODO: Include FSM, IMU, ESC (Timer CCR) headers */

void ControlLoop_Init(void) {
    /* TODO: Initialize PID controllers, EKF matrices */
}
float acc_roll;
float acc_pitch;

float roll, pitch;

void ControlLoop_Execute(ICM45686_Data_t *imu_data) {
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


/* Static state memory - must survive function exit */
    

    /* 1. Read accelerometer data as angle in degrees (multiplier 180/PI = 57.2958f) */
    acc_roll  = atan2f(imu_data->accel[1], imu_data->accel[2]) * 57.2958f;
    acc_pitch = atan2f(-imu_data->accel[0], sqrtf((float)imu_data->accel[1]*imu_data->accel[1] + (float)imu_data->accel[2]*imu_data->accel[2])) * 57.2958f;

    /* 2. Simple complementary filter (98% gyroscope, 2% accelerometer)
          Constant 0.000305f is the aggregated coefficient: (2000dps/32768) * 0.005s dt */
    roll  = 0.98f * (roll  + imu_data->gyro[0] * 0.000305f) + 0.02f * acc_roll;
    pitch = 0.98f * (pitch + imu_data->gyro[1] * 0.000305f) + 0.02f * acc_pitch;
    
    /* Variables 'roll' and 'pitch' are ready for reading in degrees */


}

void EXTI4_IRQHandler(void)
{
    /* Quick flag check and clear (Zero-Overhead) */
    if (EXTI->PR1 & EXTI_PR1_PR4)
    {
        EXTI->PR1 = EXTI_PR1_PR4; /* rc_w1 clears the flag */
        
        /* Start non-blocking DMA background transaction */
        ICM45686_StartDMAReadBurst();
    }
}
