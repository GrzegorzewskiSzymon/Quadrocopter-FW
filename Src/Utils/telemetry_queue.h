/*
 * telemetry_queue.h
 *
 *  Created on: Aug 1, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "nrf24_payload.h"

void TelemetryQueue_Init(void);
bool TelemetryQueue_Push(const NRF24_Payload_t *payload);
bool TelemetryQueue_Pop(NRF24_Payload_t *payload);