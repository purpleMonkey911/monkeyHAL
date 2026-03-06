/**
 * @file mhal_timer.h
 * 
 * @brief Timer abstraction layer for mHAL
 * 
 * @note  Timers 2 and 5 are 32-bit timers. The rest are 16-bit timers.
 * 
 * @todo Add DMA support
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_TIMER_H__
#define __MHAL_TIMER_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "mhal_status.h"
#include "mhal_dma.h"

// Used to initiate timer period to full range
#define TIMER_PERIOD_NONE  (0xFFFFFFFF)

typedef enum {
    TIMER_INDEX_1,
    TIMER_INDEX_2,
    TIMER_INDEX_3,
    TIMER_INDEX_4,
#ifdef STM32CubeG4
    TIMER_INDEX_5,
#endif
    TIMER_INDEX_6,
    TIMER_INDEX_7,
    TIMER_INDEX_8,
    TIMER_INDEX_15,
    TIMER_INDEX_16,
    TIMER_INDEX_17,
#ifdef STM32CubeG4
    TIMER_INDEX_20,
#endif
    NUM_TIMERS,
} TimerIndex;

typedef enum {
    TIMER_CHANNEL_1,
    TIMER_CHANNEL_2,
    TIMER_CHANNEL_3,
    TIMER_CHANNEL_4,
    TIMER_CHANNEL_5,
    TIMER_CHANNEL_6,
    NUM_TIMER_CHANNELS,
} TimerChannel;

/**
 * @brief Initialize Timer peripheral.
 *
 * @param [in] index   index of which Timer to initialize
 * @param [in] freq    Timer frequency
 * @param [in] period  Timer period. Use 0 or TIMER_PERIOD_NONE for full range
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_Init(TimerIndex index, uint32_t freq, uint32_t period);

/**
 * @brief Initialize Timer for quadrature encoder.
 *
 * @param [in] index   index of Timer to configure for encoder
 * @param [in] chanA   Timer channel for chanA
 * @param [in] chanB   Timer channel for chanB
 * @param [in] period  Timer period for encoder to wrap around
 *
 * @note  Must use Channel 1 and Channel 2 of selected Timer.
 * 
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_InitEncoder2(TimerIndex index, TimerChannel chanA, TimerChannel chanB, uint32_t period);

/**
 * @brief Initialize Timer for PWM output.
 *
 * @param [in] index           index of Timer to configure
 * @param [in] chan            Timer channel to configure
 * @param [in] input_freq_hz   input frequency in Hz for the PWM signal
 * @param [in] output_freq_hz  output frequency in Hz for the PWM signal
 * @param [in] duty_cycle      duty cycle percentage (0-100)
 * 
 * @note The input frequency is used to set the Timer prescaler.  Though the
 *       prescaler is allocated as 32-bit, only the lower 16 bits are valid.
 *       prescaler value is:
 * 
 *         prescaler = (TIMxCLK / input_freq_hz) - 1
 * 
 *       Input frequency should be selected that prescaler value fits within
 *       16 bits. 
 * 
 *       The output frequency is used to set the Timer period.  The period is
 *       calculated as:
 * 
 *         period = (input_freq_hz / output_freq_hz) - 1
 * 
 *        The input and output frequencies should be selected such that
 *        period value fits within 16 bits. 
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_InitPwm(TimerIndex index, TimerChannel chan, uint32_t input_freq_hz, uint32_t output_freq_hz, uint32_t duty_cycle);

/**
 * @brief Configure Timer channel for PWM output.
 *
 * @param [in] index   index of Timer to configure
 * @param [in] chan    Timer channel to configure
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_PwmConfig(TimerIndex index, TimerChannel chan);

/**
 * @brief Start Timer as a basic counter.
 *
 * @param [in] index   index of which Timer to initialize
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_Start(TimerIndex index);

/**
 * @brief Start Timer channel as PWM output. Uses DMA if configured.
 *
 * @param [in] index  index of Timer
 * @param [in] chan   Timer channel for PWM output
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_StartPwm(TimerIndex index, TimerChannel chan);

/**
 * @brief Stops PWM output for a Timer channel.
 *
 * @param [in] index   index of which Timer to stop
 * @param [in] chan    Timer channel
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_StopPwm(TimerIndex index, TimerChannel chan);

/**
 * @brief Set the PWM duty cycle
 * 
 * @param [in] index       index of which Timer to set the duty cycle
 * @param [in] chan        Timer channel
 * @param [in] duty_cycle  Duty cycle to set (0.00 to 100.00)
 * 
 * @note This function assumes the timer has been initialized with mHalTimer_InitPwm()
 * 
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_SetPwmDutyCycle(TimerIndex index, TimerChannel chan, float duty_cycle);

/**
 * @brief Start encoder2 capture.
 *
 * @param [in] index   index of which Timer to start
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_StartEncoder2(TimerIndex index);

/**
 * @brief Stop encoder2 capture.
 *
 * @param [in] index   index of which Timer to stop
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_StopEncoder2(TimerIndex index);

/**
 * @brief Sets Timer compare value.
 *
 * @param [in] index  index of Timer to set
 * @param [in] chan   Timer channel
 * @param [in] value  Timer compare value
 *
 * @return HAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_SetCompare(TimerIndex index, TimerChannel chan, uint32_t value);

/**
 * @brief Get Timer counter value.
 *
 * @param [in]  index    index of Timer
 * @param [out] counter  Timer counter value
 *
 * @return MHAL_STATUS_OK if successfully, otherwise error state
 */
extern mHalStatus mHalTimer_GetCounter(TimerIndex index, uint32_t* counter);

/**
 * @brief   Initialize and start 32-bit microsecond system timer
 *
 * @note    Since this is a 32-bit microsecond timer, the timer can be used to
 *          measure a maximum time of ~71.5 minutes
 *
 * @warning TIM2 has been utilized for this purpose.  No other modules should use TIM2
 *
 * @return  MHAL_STATUS_OK if successfully initialized otherwise MHAL error
 */
extern mHalStatus mHalTimer_SystemTimerInit(void);

/**
 * @brief Get system Timer time.
 *
 * @return 32-bit system Timer time
 */
extern uint32_t mHalTimer_GetSystemTimerTimestamp(void);


/**
 * @brief  Start timer object
 *
 * @param [out] timer  timer object to store the current system time/count
 *
 * @return MHAL_STATUS_OK if successful start, otherwise MHAL error
 */
typedef struct {
    uint32_t start_time;
} TimerObj;
extern mHalStatus mHalTimer_StartTimerObj(TimerObj* timer);

/**
 * @brief Check if timer has elapsed
 *
 * @param [out] timer  timer object from client
 * @param [in]  usec   desired elapsed time (in microseconds)
 *
 * @return true if timer has elapsed, otherwise false
 */
extern bool mHalTimer_TimerObjElapsed(TimerObj* timer, uint32_t usec);

#endif // __MHAL_TIMER_H__
