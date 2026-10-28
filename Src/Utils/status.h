/*
 * status.h
 *
 *  Created on: Oct 28, 2026
 *      Author: Szymon Grzegorzewski
 */

#pragma once

/* Common result type for driver and device APIs */
typedef enum {
    STATUS_OK = 0,
    STATUS_ERR_NO_DEVICE        /* Device did not identify itself (WHO_AM_I mismatch) */
} Status_t;
