/**
 * @file mhal_timer.h
 * 
 * @brief Timer HAL interface for monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monnkey Software. All rights reserved.
 */

#include <time.h>

#include "mhal_timer.h"

// Global timer object
static struct timespec timer_inst = {0};

uint32_t mHalTimer_GetTimeUs(void) {
    clock_gettime(CLOCK_MONOTONIC, &timer_inst);
    return (uint32_t)(timer_inst.tv_sec * 1000000 + timer_inst.tv_nsec / 1000);
}

mHalStatus mHalTimer_Start(mHalTimerObj* timer, uint32_t duration) {
    if (!timer || (duration == 0)) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    timer->start_time = mHalTimer_GetTimeUs();
    timer->duration = duration;
    timer->is_running = true;

    return MHAL_STATUS_OK;
}

mHalStatus mHalTimer_Stop(mHalTimerObj* timer) {
    if (!timer) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    timer->is_running = false;
    return MHAL_STATUS_OK;
}

bool mHalTimer_IsElapsed(mHalTimerObj* timer) {
    if (!timer || !timer->is_running) {
        return false;
    }

    uint32_t current_time = mHalTimer_GetTimeUs();
    if ((current_time - timer->start_time) >= timer->duration) {
        timer->is_running = false; // Stop the timer if it has elapsed
        return true;
    }
    return false;
}
