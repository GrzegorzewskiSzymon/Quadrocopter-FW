/*
 * dwt.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Szymon Grzegorzewski
 */

#include "dwt.h"

/* CoreSight software lock key (ARM CoreSight Architecture Specification, LAR) */
#define DWT_LAR_UNLOCK_KEY  0xC5ACCE55UL

void DWT_Init(void)
{
    /* DWT is only clocked with trace enabled (ARMv7-M ARM, DEMCR.TRCENA) */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* On Cortex-M7 the DWT registers stay write-protected until unlocked */
    DWT->LAR = DWT_LAR_UNLOCK_KEY;

    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
