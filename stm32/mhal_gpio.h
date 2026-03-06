/**
 * @file mhal_gpio.h
 * 
 * @brief GPIO abstraction layer for mHAL
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_GPIO_H__
#define __MHAL_GPIO_H__

#include <stdbool.h>
#include <stdint.h>
#include "mhal_status.h"
#include "mhal_interrupt.h"

typedef enum {
    GPIO_PORT_A,
    GPIO_PORT_B,
    GPIO_PORT_C,
    GPIO_PORT_D,
    GPIO_PORT_E,
    GPIO_PORT_F,
#if defined(STM32CubeG4)
    GPIO_PORT_G,
#endif // STM32CubeG4
    GPIO_PORT_INVALID,
} GpioPort;

typedef enum {
    GPIO_PIN0  = 0,
    GPIO_PIN1  = 1,
    GPIO_PIN2  = 2,
    GPIO_PIN3  = 3,
    GPIO_PIN4  = 4,
    GPIO_PIN5  = 5,
    GPIO_PIN6  = 6,
    GPIO_PIN7  = 7,
    GPIO_PIN8  = 8,
    GPIO_PIN9  = 9,
    GPIO_PIN10 = 10,
    GPIO_PIN11 = 11,
    GPIO_PIN12 = 12,
    GPIO_PIN13 = 13,
    GPIO_PIN14 = 14,
    GPIO_PIN15 = 15,
    NUM_GPIO_PIN,
} GpioPin;

typedef enum {
    GPIO_STATE_LOW        = 0,
    GPIO_STATE_ACTIVE_LOW = 0,
    GPIO_STATE_HIGH       = 1,
} GpioState;

typedef enum {
    GPIO_MODE_IN,             // input
    GPIO_MODE_OUT_PP,         // output push pull
    GPIO_MODE_OUT_OD,         // output open drain
    GPIO_MODE_ALT_PP,         // alternate function push pull
    GPIO_MODE_ALT_OD,         // alternate function open drain
    GPIO_MODE_ANLG,           // analog
    GPIO_MODE_INTR_RISING,    // interrupt with rising edge
    GPIO_MODE_INTR_FALLING,   // interrupt with falling edge
    GPIO_MODE_INTR_EDGE,      // interrupt with level change (rising or falling edge)
    GPIO_MODE_EVENT_RISING,   // event with rising edge
    GPIO_MODE_EVENT_FALLING,  // event with falling edge
    GPIO_MODE_EVENT_EDGE,     // event with level change (rising or falling edge)
} GpioMode;

typedef enum {
    GPIO_PULL_NONE,  // neither pull-up nor pull-down
    GPIO_PULL_UP,    // pull-up
    GPIO_PULL_DOWN,  // pull-down
} GpioPull;

typedef enum {
    GPIO_SPEED_LO,       //  < 2MHz
    GPIO_SPEED_MED,      // 4MHz to 10MHz
    GPIO_SPEED_HI,       // 10 MHz to 50MHz
#if defined(STM32CubeG4) || defined(STM32CubeL4)
    GPIO_SPEED_VERY_HI,  // 50 MHz to 120 MHz
#endif // STM32CubeG4
} GpioSpeed;

// NOTE: Some of these alternate functions are exclusive to only the F3 or G4.
//       Check the reference manual to know which functions apply to the MCU
//       in question.
typedef enum {
    // AF 0
    GPIO_AF_0_NONE     = 0,  // no alternate function selected
    GPIO_AF_0_RTC_50HZ = 0,
    GPIO_AF_0_MCO      = 0,
    GPIO_AF_0_TAMPER   = 0,
    GPIO_AF_0_SWJ      = 0,
    GPIO_AF_0_TRACE    = 0,

    // AF 1
    GPIO_AF_1_TIM2        = 1,
    GPIO_AF_1_TIM5        = 1,
    GPIO_AF_1_TIM15       = 1,
    GPIO_AF_1_TIM16       = 1,
    GPIO_AF_1_TIM17       = 1,
    GPIO_AF_1_TIM17_COMP1 = 1,
    GPIO_AF_1_LPTIM1      = 1,
    GPIO_AF_1_EVENTOUT    = 1,
    GPIO_AF_1_IR          = 1,

    // AF 2
    GPIO_AF_2_TIM1        = 2,
    GPIO_AF_2_TIM2        = 2,
    GPIO_AF_2_TIM3        = 2,
    GPIO_AF_2_TIM4        = 2,
    GPIO_AF_2_TIM5        = 2,
    GPIO_AF_2_TIM8        = 2,
    GPIO_AF_2_TIM15       = 2,
    GPIO_AF_2_TIM16       = 2,
    GPIO_AF_2_TIM20       = 2,
    GPIO_AF_2_COMP1       = 2,
    GPIO_AF_2_TIM1_COMP1  = 2,
    GPIO_AF_2_TIM15_COMP1 = 2,
    GPIO_AF_2_TIM16_COMP1 = 2,
    GPIO_AF_2_TIM20_COMP1 = 2,
    GPIO_AF_2_TIM20_COMP2 = 2,
    GPIO_AF_2_I2C3        = 2,

    // AF 3
    GPIO_AF_3_TSC     = 3,
    GPIO_AF_3_TIM8    = 3,
    GPIO_AF_3_TIM20   = 3,
    GPIO_AF_3_COMP7   = 3,
    GPIO_AF_3_TIM15   = 3,
    GPIO_AF_3_UCPD1   = 3,
    GPIO_AF_3_I2C3    = 3,
    GPIO_AF_3_I2C4    = 3,
    GPIO_AF_3_HRTIM1  = 3,
    GPIO_AF_3_QUADSPI = 3,
    GPIO_AF_3_SAI1    = 3,
    GPIO_AF_3_COMP3   = 3,

    // AF 4
    GPIO_AF_4_TIM1       = 4,
    GPIO_AF_4_TIM8       = 4,
    GPIO_AF_4_TIM16      = 4,
    GPIO_AF_4_TIM17      = 4,
    GPIO_AF_4_TIM8_COMP1 = 4,
    GPIO_AF_4_I2C1       = 4,
    GPIO_AF_4_I2C2       = 4,
    GPIO_AF_4_I2C3       = 4,
    GPIO_AF_4_I2C4       = 4,

    // AF 5
    GPIO_AF_5_SPI1       = 5,
    GPIO_AF_5_SPI2       = 5,
    GPIO_AF_5_SPI3       = 5,
    GPIO_AF_5_SPI4       = 5,
    GPIO_AF_5_I2S        = 5,
    GPIO_AF_5_I2S2EXT    = 5,
    GPIO_AF_5_TIM8       = 5,
    GPIO_AF_5_TIM8_COMP1 = 5,
    GPIO_AF_5_IR         = 5,
    GPIO_AF_5_UART4      = 5,
    GPIO_AF_5_UART5      = 5,

    // AF 6
    GPIO_AF_6_SPI2       = 6,
    GPIO_AF_6_SPI3       = 6,
    GPIO_AF_6_I2S3EXT    = 6,
    GPIO_AF_6_TIM1       = 6,
    GPIO_AF_6_TIM5       = 6,
    GPIO_AF_6_TIM8       = 6,
    GPIO_AF_6_TIM20      = 6,
    GPIO_AF_6_TIM1_COMP1 = 6,
    GPIO_AF_6_TIM1_COMP2 = 6,
    GPIO_AF_6_TIM8_COMP2 = 6,
    GPIO_AF_6_IR         = 6,

    // AF 7
    GPIO_AF_7_USART1 = 7,
    GPIO_AF_7_USART2 = 7,
    GPIO_AF_7_USART3 = 7,
    GPIO_AF_7_COMP3  = 7,
    GPIO_AF_7_COMP5  = 7,
    GPIO_AF_7_COMP6  = 7,
    GPIO_AF_7_COMP7  = 7,
    GPIO_AF_7_CAN    = 7,

    // AF 8
    GPIO_AF_8_COMP1   = 8,
    GPIO_AF_8_COMP2   = 8,
    GPIO_AF_8_COMP3   = 8,
    GPIO_AF_8_COMP4   = 8,
    GPIO_AF_8_COMP5   = 8,
    GPIO_AF_8_COMP6   = 8,
    GPIO_AF_8_COMP7   = 8,
    GPIO_AF_8_I2C3    = 8,
    GPIO_AF_8_I2C4    = 8,
    GPIO_AF_8_LPUART1 = 8,
    GPIO_AF_8_UART4   = 8,
    GPIO_AF_8_UART5   = 8,

    // AF 9
    GPIO_AF_9_CAN         = 9,
    GPIO_AF_9_FDCAN1      = 9,
    GPIO_AF_9_FDCAN2      = 9,
    GPIO_AF_9_TIM1        = 9,
    GPIO_AF_9_TIM8        = 9,
    GPIO_AF_9_TIM15       = 9,
    GPIO_AF_9_TIM1_COMP1  = 9,
    GPIO_AF_9_TIM8_COMP1  = 9,
    GPIO_AF_9_TIM15_COMP1 = 9,

    // AF 10
    GPIO_AF_10_TIM2        = 10,
    GPIO_AF_10_TIM3        = 10,
    GPIO_AF_10_TIM4        = 10,
    GPIO_AF_10_TIM8        = 10,
    GPIO_AF_10_TIM17       = 10,
    GPIO_AF_10_TIM8_COMP2  = 10,
    GPIO_AF_10_TIM17_COMP2 = 10,
    GPIO_AF_10_QUADSPI     = 10,

    // AF 11
    GPIO_AF_11_FDCAN1     = 11,
    GPIO_AF_11_FDCAN3     = 11,
    GPIO_AF_11_TIM1       = 11,
    GPIO_AF_11_TIM8       = 11,
    GPIO_AF_11_TIM8_COMP1 = 11,
    GPIO_AF_11_LPTIM1     = 11,

    // AF 12
    GPIO_AF_12_LPUART1    = 12,
    GPIO_AF_12_TIM1       = 12,
    GPIO_AF_12_TIM1_COMP1 = 12,
    GPIO_AF_12_TIM1_COMP2 = 12,
    GPIO_AF_12_HRTIM1     = 12,
    GPIO_AF_12_FMC        = 12,
    GPIO_AF_12_SAI1       = 12,

    // AF 13
    GPIO_AF_13_HRTIM1 = 13,
    GPIO_AF_13_SAI1   = 13,

    // AF 14
    GPIO_AF_14_USB = 14,
    GPIO_AF_14_TIM2 = 14,
    GPIO_AF_14_TIM15 = 14,
    GPIO_AF_14_UCPD1 = 14,
    GPIO_AF_14_SAI1  = 14,
    GPIO_AF_14_UART4 = 14,
    GPIO_AF_14_UART5 = 14,

    // AF 15
    GPIO_AF_15_EVENT_OUT = 15,

} GpioAltFunc;

typedef struct _gpio_t gpio_t;
typedef gpio_t* Gpio;

typedef struct {
    GpioPort port;
    GpioPin pin;
    GpioMode mode;
    GpioPull pull;
    GpioSpeed speed;
    GpioAltFunc alt_func;
} GpioConfig;

typedef void (*mHalGpio_ExternalInterruptCb)(void* ctx);

/**
 * @brief Initialize GPIO
 * 
 * @param [in] pGpio     pointer to GPIO instance
 * @param [in] config    GPIO configuration
 * 
 * @return  MHAL_STATUS_OK if successful, otherwise mHAL error
 */
extern mHalStatus mHalGpio_Init(Gpio* pGpio, GpioConfig* config);

/**
 * @brief Enable/Disable external interrupt.
 *        A non-NULL callback parameter enables the external interrupt.
 *        A NULL callback parameter disables the external interrupt.
 *
 * @param [in] pin       external interrupt pin number
 * @param [in] priority  interrupt preempt priority
 * @param [in] callback  external interrupt callback function
 * @param [in] ctx       context for the callback function
 *
 * @return HAL_STATUS_OK if successful, otherwise error code
 */
extern mHalStatus HalGpio_EnableExtInterrupt(Gpio gpio, mHalInterruptPriority priority,
    mHalGpio_ExternalInterruptCb callback, void* ctx);

/**
 * @brief Set GPIO state
 *
 * @param [in] gpio   GPIO instance
 * @param [in] state  State to assign to this GPIO
 *
 * @return  HAL_STATUS_OK if write is successful, otherwise error
 */
extern mHalStatus mHalGpio_Write(Gpio gpio, GpioState state);

/**
 * @brief Read GPIO state
 *
 * @param [in]  gpio   GPIO instance
 * @param [out] state  State of this GPIO
 *
 * @return  MHAL_STATUS_OK if state return is valid, otherwise mHAL error
 */
extern mHalStatus mHalGpio_Read(Gpio gpio, GpioState* state);

/**
 * @brief Toggle GPIO state
 *
 * @param [in] gpio   GPIO instance
 *
 * @return  MHAL_STATUS_OK if toggle is successful, otherwise mHAL error
 */
extern mHalStatus mHalGpio_Toggle(Gpio gpio);

#endif // __MHAL_GPIO_H__
