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
    uint32_t duration;   // Duration in microseconds
    bool is_running;     // Timer running state
} mHalTimerObj;


/**
 * @brief Get the current time in microseconds.
 * 
 * @return Current time in microseconds since the epoch
 * 
 * @note This function uses CLOCK_MONOTONIC to get the time, which is not affected by system clock changes.
 */
extern uint32_t mHalTimer_GetTimeUs(void);

/**
 * @brief Start a timer with a specified duration.
 * 
 * @param timer Pointer to the mHalTimerObj to start
 * @param duration Duration in microseconds for the timer
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalTimer_Start(mHalTimerObj* timer, uint32_t duration);

/**
 * @brief Stop a running timer.
 * 
 * @param timer Pointer to the HalTimerObj to stop
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalTimer_Stop(mHalTimerObj* timer);

/**
 * @brief Check if the timer has elapsed.
 * 
 * @param timer Pointer to the mHalTimerObj to check
 * 
 * @return true if the timer has elapsed, otherwise false
 */
extern bool mHalTimer_IsElapsed(mHalTimerObj* timer);

#endif // __MHAL_TIMER_H__
