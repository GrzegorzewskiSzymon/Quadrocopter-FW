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
    STATUS_ERR_NO_DEVICE,       /* Device did not identify itself (WHO_AM_I mismatch) */
    STATUS_ERR_TIMEOUT,         /* Hardware flag did not change within its time budget */
    STATUS_ERR_BUSY             /* Resource is in use, request was not started */
} Status_t;
