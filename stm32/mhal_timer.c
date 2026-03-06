/**
 * @file mhal_timer.c
 * 
 * @brief Timer abstraction layer for mHAL
 * 
 * @note  Timers 2 and 5 are 32-bit timers. The rest are 16-bit timers.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef _UNITTEST
  #if defined(STM32CubeF3)
    #include "stm32f3xx_hal.h"
    #include "stm32f3xx_hal_rcc.h"
  #elif defined(STM32CubeG4)
    #include "stm32g4xx_hal.h"
    #include "stm32g4xx_hal_rcc.h"
  #elif defined(STM32CubeL4)
    #include "stm32l4xx_hal.h"
    #include "stm32l4xx_hal_rcc.h"
  #else
    #error "MCU not supported!"
  #endif
#else
  #include "mttr-hal-mocks.h"
#endif  // _UNITTEST

#include "_mhal_internal.h"
#include "mhal_timer.h"

#define MICROSECS_PER_SEC (1000000UL)

typedef struct {
    TIM_HandleTypeDef hTimer;          // STM32Cube Timer handle
} Timer;

static Timer timer_inst[NUM_TIMERS] = {0};


/**
 * @brief Get hardware-specific Timer peripheral.
 *
 * @param [in] index  index of which Timer to retrieve
 *
 * @return pointer to Timer peripheral
 */
static TIM_TypeDef* GetTimerBase(TimerIndex index) {
    switch (index) {
        case TIMER_INDEX_1:  return TIM1;
        case TIMER_INDEX_2:  return TIM2;
        case TIMER_INDEX_3:  return TIM3;
        case TIMER_INDEX_4:  return TIM4;
#ifdef STM32CubeG4
        case TIMER_INDEX_5:  return TIM5;
#endif
        case TIMER_INDEX_6:  return TIM6;
        case TIMER_INDEX_7:  return TIM7;
        case TIMER_INDEX_8:  return TIM8;
        case TIMER_INDEX_15: return TIM15;
        case TIMER_INDEX_16: return TIM16;
        case TIMER_INDEX_17: return TIM17;
#ifdef STM32CubeG4
        case TIMER_INDEX_20: return TIM20;
#endif
        default:             return NULL;
    }
}

/**
 * @brief Get hardware-specific Timer channel.
 *
 * @param [in] chan  Timer channel enum value
 *
 * @return hardware-specific Timer channel
 */
static uint32_t GetChannelCode(TimerChannel chan) {
    switch (chan) {
        case TIMER_CHANNEL_1:  return TIM_CHANNEL_1;
        case TIMER_CHANNEL_2:  return TIM_CHANNEL_2;
        case TIMER_CHANNEL_3:  return TIM_CHANNEL_3;
        case TIMER_CHANNEL_4:  return TIM_CHANNEL_4;
        case TIMER_CHANNEL_5:  return TIM_CHANNEL_5;
        case TIMER_CHANNEL_6:  return TIM_CHANNEL_6;
        default:               return TIM_CHANNEL_ALL;
    }
}

/**
 * @brief Enable Timer peripheral clock.
 *
 * @param [in] index  index of which Timer clock to enable
 */
static void TimerClockEnable(TimerIndex index) {
    switch (index) {
        case TIMER_INDEX_1:  __HAL_RCC_TIM1_CLK_ENABLE();   break;
        case TIMER_INDEX_2:  __HAL_RCC_TIM2_CLK_ENABLE();   break;
        case TIMER_INDEX_3:  __HAL_RCC_TIM3_CLK_ENABLE();   break;
        case TIMER_INDEX_4:  __HAL_RCC_TIM4_CLK_ENABLE();   break;
#ifdef STM32CubeG4
        case TIMER_INDEX_5:  __HAL_RCC_TIM5_CLK_ENABLE();   break;
#endif
        case TIMER_INDEX_6:  __HAL_RCC_TIM6_CLK_ENABLE();   break;
        case TIMER_INDEX_7:  __HAL_RCC_TIM7_CLK_ENABLE();   break;
        case TIMER_INDEX_8:  __HAL_RCC_TIM8_CLK_ENABLE();   break;
        case TIMER_INDEX_15: __HAL_RCC_TIM15_CLK_ENABLE();  break;
        case TIMER_INDEX_16: __HAL_RCC_TIM16_CLK_ENABLE();  break;
        case TIMER_INDEX_17: __HAL_RCC_TIM17_CLK_ENABLE();  break;
#ifdef STM32CubeG4
        case TIMER_INDEX_20: __HAL_RCC_TIM20_CLK_ENABLE();  break;
#endif
        default: break;
    }
}

/**
 * @brief Get Timer interrupt request vector.
 *
 * @param [in] index  index of Timer
 *
 * @return Timer interrupt request vector
 */
__attribute__((used))
static IRQn_Type GetTimerIRQn(TimerIndex index) {
    switch (index) {
        case TIMER_INDEX_1:  return TIM1_CC_IRQn;
        case TIMER_INDEX_2:  return TIM2_IRQn;
        case TIMER_INDEX_3:  return TIM3_IRQn;
        case TIMER_INDEX_4:  return TIM4_IRQn;
#ifdef STM32CubeG4
        case TIMER_INDEX_5:  return TIM5_IRQn;
#endif
        case TIMER_INDEX_6:  return TIM6_DAC_IRQn;
#ifdef STM32CubeG4
        case TIMER_INDEX_7:  return TIM7_DAC_IRQn;
#else
        case TIMER_INDEX_7:  return TIM7_IRQn;
#endif
        case TIMER_INDEX_8:  return TIM8_CC_IRQn;
        case TIMER_INDEX_15: return TIM1_BRK_TIM15_IRQn;
        case TIMER_INDEX_16: return TIM1_UP_TIM16_IRQn;
        case TIMER_INDEX_17: return TIM1_TRG_COM_TIM17_IRQn;
#ifdef STM32CubeG4
        case TIMER_INDEX_20: return TIM20_CC_IRQn;
#endif
        default:             return 0;
    }
}

/**
 * @brief Get Timer clock speed.
 *
 * @param [in] index  index of Timer
 *
 * @return Timer clock speed
 */
static uint32_t GetTimerClockSpeed(TimerIndex index) {
    uint32_t clk_speed = 0UL;
    uint32_t clk_div = 0UL;
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    uint32_t pFLatency;

    HAL_RCC_GetClockConfig(&RCC_ClkInitStruct, &pFLatency);

    switch (index) {
        case TIMER_INDEX_2:
        case TIMER_INDEX_3:
        case TIMER_INDEX_4:
#ifdef STM32CubeG4
        case TIMER_INDEX_5:
#endif
        case TIMER_INDEX_6:
        case TIMER_INDEX_7:
            clk_speed = HAL_RCC_GetPCLK1Freq();
            clk_div = RCC_ClkInitStruct.APB1CLKDivider;
            break;
        case TIMER_INDEX_1:
        case TIMER_INDEX_8:
        case TIMER_INDEX_15:
        case TIMER_INDEX_16:
        case TIMER_INDEX_17:
#ifdef STM32CubeG4
        case TIMER_INDEX_20:
#endif
            clk_speed = HAL_RCC_GetPCLK2Freq();
            clk_div = RCC_ClkInitStruct.APB2CLKDivider;
            break;

        default:
            break;
    }

    // If APBx prescaler is not 1, APB clock is doubled to the APBx timers
    if (clk_div != RCC_HCLK_DIV1) {
        clk_speed *= 2;
    }
    return clk_speed;
}

/**
 * @brief Get Timer instance.
 *
 * @param [in] index  index of Timer
 *
 * @return Timer instance if index is valid, NULL otherwise
 */
static Timer* GetTimer(TimerIndex index) {
    if (index >= NUM_TIMERS) {
        return NULL;
    }
    return &timer_inst[index];
}

/**
 * @brief Check if Timer is enabled.
 *
 * @param [in] pTimer  pointer to Timer
 *
 * @return true if enabled, otherwise false
 */
static bool IsTimerEnabled(Timer* pTimer) {
    if (!pTimer) {
        return false;
    }
    return (pTimer->hTimer.Instance != NULL);
}

/**
 * @brief Get Timer instance from STM32Cube Timer handle.
 *
 * @param [in] hTimer  pointer to STM32Cube Timer handle
 *
 * @return Timer instance if valid handle, else NULL
 */
static Timer* GetTimerFromHandle(TIM_HandleTypeDef* hTimer) {
    if (!hTimer) {
        return NULL;
    }

    Timer* retval = NULL;

    for (uint8_t index = 0; index < NUM_TIMERS; index++) {
        if (hTimer == &timer_inst[index].hTimer) {
            retval = &timer_inst[index];
            break;
        }
    }
    return retval;
}

mHalStatus mHalTimer_Init(TimerIndex index, uint32_t freq, uint32_t period) {
    if (index >= NUM_TIMERS) {
        return HAL_STATUS_ERROR_INSTANCE;
    }

    if (period == 0) {
        period = TIMER_PERIOD_NONE;
    }

    Timer* pTimer = &timer_inst[index];
    pTimer->hTimer.Instance               = GetTimerBase(index);
    pTimer->hTimer.Init.Period            = period;
    // Prescaler is allocated as 32-bit but only lower 16 bits are valid
    pTimer->hTimer.Init.Prescaler         = (freq) ? (((uint32_t)(GetTimerClockSpeed(index)/ freq) - 1) & 0xFFFF) : 0;
    pTimer->hTimer.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    pTimer->hTimer.Init.CounterMode       = TIM_COUNTERMODE_UP;
    pTimer->hTimer.Init.RepetitionCounter = 0;
    pTimer->hTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    TimerClockEnable(index);
    return _mhal_translateStatus( HAL_TIM_Base_Init(&pTimer->hTimer) );
}

mHalStatus mHalTimer_InitEncoder2(TimerIndex index, TimerChannel chanA, TimerChannel chanB, uint32_t period) {
    // NOTE: STM32 implementation of 2-output encoder requires chanA and chanB
    //       to be allocated to TIMER_CHANNEL_1 and TIMER_CHANNEL_2, respectively.
    if ((chanA != TIMER_CHANNEL_1) || (chanB != TIMER_CHANNEL_2)) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // For encoder mode to work, the timer prescaler must be set to 0 by
    // indirectly setting the frequency argument to 0.
    mHalStatus status = mHalTimer_Init(index, 0, period);
    if (status != MHAL_STATUS_OK) {
        return status;
    }

    // index already verified to be valid in HalTimer_Init()
    Timer* pTimer = &timer_inst[index];
    TIM_Encoder_InitTypeDef encoder = {0};
    encoder.EncoderMode = TIM_ENCODERMODE_TI12;
    encoder.IC1Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC1Prescaler = TIM_ICPSC_DIV1;
    encoder.IC1Filter = 5;
    encoder.IC2Polarity = TIM_ICPOLARITY_RISING;
    encoder.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    encoder.IC2Prescaler = TIM_ICPSC_DIV1;
    encoder.IC2Filter = 5;
    return _mhal_translateStatus(HAL_TIM_Encoder_Init(&pTimer->hTimer, &encoder));
}

mHalStatus HalTimer_InitPwm(TimerIndex index, TimerChannel chan, uint32_t input_freq_hz, uint32_t output_freq_hz, uint32_t duty_cycle) {
    if (index >= NUM_TIMERS || chan >= NUM_TIMER_CHANNELS) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    Timer* pTimer = &timer_inst[index];
    pTimer->hTimer.Instance               = GetTimerBase(index);
    // Prescaler and Period are allocated as 32-bit but only lower 16 bits are valid
    pTimer->hTimer.Init.Prescaler         = ((uint32_t)(GetTimerClockSpeed(index)/input_freq_hz) - 1) & 0xFFFF;
    pTimer->hTimer.Init.Period            = (input_freq_hz/output_freq_hz - 1) & 0xFFFF;
    pTimer->hTimer.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
    pTimer->hTimer.Init.CounterMode       = TIM_COUNTERMODE_UP;
    pTimer->hTimer.Init.RepetitionCounter = 0;
    pTimer->hTimer.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

    TimerClockEnable(index);
    mHalStatus status = _mhal_translateStatus(HAL_TIM_PWM_Init(&pTimer->hTimer));
    if (status != MHAL_STATUS_OK) {
        return status;
    }

    // Set up PWM channel
    TIM_OC_InitTypeDef conf = {0};
    conf.OCMode       = TIM_OCMODE_PWM1;
    conf.Pulse        = (duty_cycle * pTimer->hTimer.Init.Period) / 100; // Convert duty cycle to pulse width
    conf.OCPolarity   = TIM_OCPOLARITY_HIGH;
    conf.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
    conf.OCFastMode   = TIM_OCFAST_DISABLE;
    conf.OCIdleState  = TIM_OCIDLESTATE_RESET;
    conf.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    return _mhal_translateStatus(HAL_TIM_PWM_ConfigChannel(&pTimer->hTimer, &conf, GetChannelCode(chan)));
}

mHalStatus mHalTimer_PwmConfig(TimerIndex index, TimerChannel chan) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));

    if (HAL_TIM_PWM_Init(&timer->hTimer) != HAL_OK) {
        return HAL_STATUS_ERROR;
    }

    TIM_OC_InitTypeDef conf = {0};
    conf.OCMode = TIM_OCMODE_PWM1;
    conf.Pulse = 0;
    conf.OCPolarity = TIM_OCPOLARITY_HIGH;
    conf.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    conf.OCFastMode = TIM_OCFAST_DISABLE;
    conf.OCIdleState = TIM_OCIDLESTATE_RESET;
    conf.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    return _mhal_translateStatus( HAL_TIM_PWM_ConfigChannel(&timer->hTimer, &conf, GetChannelCode(chan)) );
}

mHalStatus HalTimer_Start(TimerIndex index) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    return _hal_translateStatus( HAL_TIM_Base_Start(&timer->hTimer) );
}

mHalStatus HalTimer_StartPwm(TimerIndex index, TimerChannel chan) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    if (chan >= NUM_TIMER_CHANNELS) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    HAL_StatusTypeDef status;
    uint32_t chan_code = GetChannelCode(chan);

    return _mhal_translateStatus(HAL_TIM_PWM_Start(&timer->hTimer, chan_code));
}

mHalStatus HalTimer_StopPwm(TimerIndex index, TimerChannel chan) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    if (chan >= NUM_TIMER_CHANNELS) {
        return HAL_STATUS_ERROR_INVALID_PARAM;
    }

    uint32_t chan_code = GetChannelCode(chan);
    return _hal_translateStatus(HAL_TIM_PWM_Stop(&timer->hTimer, chan_code));
}

mHalStatus HalTimer_SetPwmDutyCycle(TimerIndex index, TimerChannel chan, float duty_cycle) {
    Timer* timer = GetTimer(index);
    if (!timer || (duty_cycle < 0.0f) || (duty_cycle > 100.0f)) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    if (chan >= NUM_TIMER_CHANNELS) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Calculate pulse width based on duty cycle and Timer period
    uint32_t pulse_width = (duty_cycle * (float)timer->hTimer.Init.Period) / 100.0f;
    __HAL_TIM_SET_COMPARE(&timer->hTimer, GetChannelCode(chan), pulse_width);
    
    return MHAL_STATUS_OK;
}

mHalStatus HalTimer_StartEncoder2(TimerIndex index) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    return _mhal_translateStatus(HAL_TIM_Encoder_Start(&timer->hTimer, TIM_CHANNEL_ALL));
}

mHalStatus HalTimer_StopEncoder2(TimerIndex index) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    return _mhal_translateStatus(HAL_TIM_Encoder_Stop(&timer->hTimer, TIM_CHANNEL_ALL));
}

mHalStatus HalTimer_SetCompare(TimerIndex index, TimerChannel chan, uint32_t value) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    if (chan >= NUM_TIMER_CHANNELS) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    __HAL_TIM_SET_COMPARE(&timer->hTimer, GetChannelCode(chan), value);
    return MHAL_STATUS_OK;
}

mHalStatus HalTimer_GetCounter(TimerIndex index, uint32_t* counter) {
    Timer* timer = GetTimer(index);
    _HAL_VALIDATE_DEV(timer, IsTimerEnabled(timer));
    *counter = __HAL_TIM_GET_COUNTER(&timer->hTimer);
    return MHAL_STATUS_OK;
}

mHalStatus HalTimer_SystemTimerInit(void) {
    _HAL_RETURN_ON_ERROR( HalTimer_Init(TIMER_INDEX_2, MICROSECS_PER_SEC, TIMER_PERIOD_NONE) );
    _HAL_RETURN_ON_ERROR( HalTimer_Start(TIMER_INDEX_2) );
    return MHAL_STATUS_OK;
}

uint32_t HalTimer_GetSystemTimerTimestamp(void) {
    uint32_t counter = 0;
    HalTimer_GetCounter(TIMER_INDEX_2, &counter);
    return counter;
}

mHalStatus HalTimer_StartTimerObj(TimerObj* timer) {
    if (timer) {
        timer->start_time = HalTimer_GetSystemTimerTimestamp();
        return MHAL_STATUS_OK;
    }
    return MHAL_STATUS_ERROR;
}

bool HalTimer_TimerObjElapsed(TimerObj* timer, uint32_t usec) {
    if (timer) {
        return (HalTimer_GetSystemTimerTimestamp() - timer->start_time  >= usec);
    }
    return false;
}


/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief Hardware interrupt handler for Timer1
  */
void TIM1_CC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_1].hTimer);
}

/**
  * @brief Hardware interrupt handler for Timer2
  */
void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_2].hTimer);
}

/**
  * @brief Hardware interrupt handler for Timer3
  */
void TIM3_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_3].hTimer);
}

/**
  * @brief Hardware interrupt handler for Timer4
  */
void TIM4_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_4].hTimer);
}

#ifdef STM32CubeG4
/**
  * @brief Hardware interrupt handler for Timer5
  */
void TIM5_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_5].hTimer);
}
#endif

/**
  * @brief Hardware interrupt handler for Timer6
  */
void TIM6_DAC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_6].hTimer);
}

/**
  * @brief Hardware interrupt handler for Timer7
  */
#ifdef STM32CubeG4
void TIM7_DAC_IRQHandler(void) {
#else
void TIM7_IRQHandler(void) {
#endif
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_7].hTimer);
}

/**
  * @brief Hardware interrupt handler for Timer8
  */
void TIM8_CC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_8].hTimer);
}

#ifdef STM32CubeG4
/**
  * @brief Hardware interrupt handler for Timer20
  */
void TIM20_CC_IRQHandler(void) {
    HAL_TIM_IRQHandler(&timer_inst[TIMER_INDEX_20].hTimer);
}
#endif
