/*
 * nrf24_payload.h
 *
 *  Created on: Aug 1, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once
#include <stdint.h>

/* Packet Identification */
typedef enum {
    NRF24_PACKET_DEBUG_IMU  = 0x01U,
    NRF24_PACKET_SYS_STATUS = 0x02U
    /* Add future packet types here */
} nrf24_packet_type_t;

/* IMU Data Dump Payload (18 bytes) */
typedef struct {
    uint32_t timestamp_ms;
    int16_t  accel[3];
    int16_t  gyro[3];
    int16_t  temp;
} nrf24_payload_imu_t;

/* Unified 32-byte Structure */
typedef struct {
    /* Header (4 bytes) - Ensures 32-bit boundary alignment for data */
    uint8_t  packet_type;
    uint8_t  seq_num;
    uint16_t reserved;
    
    /* Payload space (28 bytes) */
    union {
        nrf24_payload_imu_t imu;
        uint8_t             raw[28]; /* Forces exactly 28 bytes reservation */
    } data;
} NRF24_Payload_t;

_Static_assert(sizeof(NRF24_Payload_t) == 32U, "NRF24 payload must match the 32-byte static RX_PW width");