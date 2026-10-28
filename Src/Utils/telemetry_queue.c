/*
 * telemetry_queue.c
 *
 *  Created on: Aug 1, 2026
 *      Author: Szymon Grzegorzewski
 */

 
#include "telemetry_queue.h"
#include "stm32h723xx.h" /* For __get_PRIMASK, __disable_irq, __set_PRIMASK */

/* Size MUST be a power of 2 for bitwise masking */
#define QUEUE_SIZE 32U
#define QUEUE_MASK (QUEUE_SIZE - 1U)

static NRF24_Payload_t queue[QUEUE_SIZE];
static volatile uint8_t head = 0U;
static volatile uint8_t tail = 0U;

void TelemetryQueue_Init(void)
{
    head = 0U;
    tail = 0U;
}

bool TelemetryQueue_Push(const NRF24_Payload_t *payload)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq(); /* Enter critical section */

    uint8_t next_head = (head + 1U) & QUEUE_MASK;
    if (next_head == tail)
    {
        __set_PRIMASK(primask); /* Exit critical section */
        return false;           /* Queue is full - drop packet */
    }

    queue[head] = *payload; /* Struct copy (32 bytes aligned) */
    head = next_head;

    __set_PRIMASK(primask); /* Exit critical section */
    return true;
}

bool TelemetryQueue_Pop(NRF24_Payload_t *payload)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq(); /* Enter critical section */

    if (head == tail)
    {
        __set_PRIMASK(primask);
        return false; /* Queue is empty */
    }

    *payload = queue[tail];
    tail = (tail + 1U) & QUEUE_MASK;

    __set_PRIMASK(primask); /* Exit critical section */
    return true;
}