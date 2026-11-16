/**
 * @file mhal_timer.h
 * 
 * @brief Timer HAL interface for monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monnkey Software. All rights reserved.
 */

#ifndef __MHAL_TIMER_H__
#define __MHAL_TIMER_H__

#include <stdint.h>
#include <stdbool.h>

#include "mhal_status.h"

typedef struct {
    uint32_t start_time; // Start time in microseconds
} mHalTimerObj;


/**
 * @brief Initialize the system timer.
 *
 * @note On Linux, CLOCK_MONOTONIC needs no setup, so this is a no-op provided
 *       for API compatibility with other platforms.
 *
 * @return MHAL_STATUS_OK
 */
extern mHalStatus mHalTimer_SystemTimerInit(void);

/**
 * @brief Get the current time in microseconds.
 * 
 * @return Current time in microseconds
 * 
 * @note This function uses CLOCK_MONOTONIC to get the time, which is not affected by system clock changes.
 */
extern uint32_t mHalTimer_GetSystemTimerTimestamp(void);

/**
 * @brief Start a timer.
 * 
 * @param timer Pointer to the mHalTimerObj to start
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalTimer_StartTimerObj(mHalTimerObj* timer);

/**
 * @brief Check if the timer has elapsed.
 * 
 * @param timer Pointer to the mHalTimerObj to check
 * @param usec  desired elapsed time (in microseconds)
 * 
 * @return true if the timer has elapsed, otherwise false
 *
 * @note Since this is a 32-bit microsecond timer, the timer can be used to
 *       measure a maximum time of ~71.5 minutes
 */
extern bool mHalTimer_TimerObjElapsed(mHalTimerObj* timer, uint32_t usec);

#endif // __MHAL_TIMER_H__
