/**
 * @file mhal_timer.c
 * 
 * @brief Timer HAL interface for monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monnkey Software. All rights reserved.
 */

#include <time.h>

#include "mhal_timer.h"

mHalStatus mHalTimer_SystemTimerInit(void) {
    return MHAL_STATUS_OK;
}

uint32_t mHalTimer_GetSystemTimerTimestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000);
}

mHalStatus mHalTimer_StartTimerObj(mHalTimerObj* timer) {
    if (!timer) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    timer->start_time = mHalTimer_GetSystemTimerTimestamp();

    return MHAL_STATUS_OK;
}

bool mHalTimer_TimerObjElapsed(mHalTimerObj* timer, uint32_t usec) {
    if (timer) {
        return ((mHalTimer_GetSystemTimerTimestamp() - timer->start_time) >= usec);
    }
    return false;
}
