/**
 * @file hal_gpio.c
 * 
 * @brief GPIO abstraction layer for mHAL
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef _UNITTEST
  #if defined(STM32CubeF3)
    #include "stm32f3xx_hal.h"
  #elif defined(STM32CubeG4)
    #include "stm32g4xx_hal.h"
  #elif defined(STM32CubeL4)
    #include "stm32l4xx_hal.h"
  #else
    #error "MCU not supported!"
  #endif
#else
  #include "mttr-hal-mocks.h"
#endif // _UINTTEST

#include "_mhal_internal.h"
#include "mhal_gpio.h"

// User EXTI (external interrupt) callback functions
static struct {
    HalGpio_ExternalInterrupt callback;
    void* ctx;
 } exti_cb_[NUM_GPIO_PIN];

typedef struct _gpio_t {
    GpioPort port;
    GpioPin pin;
} gpio_t;


/**
 * @brief Maps mHAL GPIO port to hardware-specific GPIO port
 *
 * @param [in] port   GPIO port
 *
 * @return  equivalent hardware-specific HAL GPIO port
 *
 * @todo enable GPIOG and GPIOH when we have an MCU that supports them
 */
static GPIO_TypeDef* GetGpioPort(GpioPort port) {
    GPIO_TypeDef* retval = NULL;

    switch (port) {
    case GPIO_PORT_A:
        retval = GPIOA;
        break;
    case GPIO_PORT_B:
        retval = GPIOB;
        break;
    case GPIO_PORT_C:
        retval = GPIOC;
        break;
    case GPIO_PORT_D:
        retval = GPIOD;
        break;
    case GPIO_PORT_E:
        retval = GPIOE;
        break;
    case GPIO_PORT_F:
        retval = GPIOF;
        break;
#if defined(STM32CubeG4)
    case GPIO_PORT_G:
        retval = GPIOG;
        break;
#endif // STM32CubeG4
    default:
        break;
    }

    return retval;
}

/**
 * @brief Get external interrupt request vector.
 *
 * @param [in] pin  external interrupt pin
 *
 * @return External interrupt request vector
 */
static IRQn_Type GetExtiIRQn(GpioPin pin) {
    switch (pin) {
        case GPIO_PIN0:  return EXTI0_IRQn;
        case GPIO_PIN1:  return EXTI1_IRQn;
#if defined(STM32CubeF3)
        case GPIO_PIN2:  return EXTI2_TSC_IRQn;
#else
        case GPIO_PIN2:  return EXTI2_IRQn;
#endif
        case GPIO_PIN3:  return EXTI3_IRQn;
        case GPIO_PIN4:  return EXTI4_IRQn;
        case GPIO_PIN5:  return EXTI9_5_IRQn;
        case GPIO_PIN6:  return EXTI9_5_IRQn;
        case GPIO_PIN7:  return EXTI9_5_IRQn;
        case GPIO_PIN8:  return EXTI9_5_IRQn;
        case GPIO_PIN9:  return EXTI9_5_IRQn;
        case GPIO_PIN10: return EXTI15_10_IRQn;
        case GPIO_PIN11: return EXTI15_10_IRQn;
        case GPIO_PIN12: return EXTI15_10_IRQn;
        case GPIO_PIN13: return EXTI15_10_IRQn;
        case GPIO_PIN14: return EXTI15_10_IRQn;
        case GPIO_PIN15: return EXTI15_10_IRQn;
        default:         return 0;
    }
}

/**
 * @brief Maps mHAL GPIO pin to hardware-specific GPIO pin
 *
 * @param [in] pin   mHAL GPIO pin
 *
 * @return  equivalent hardware-specific HAL GPIO pin
 */
static inline uint32_t GetGpioPin(GpioPin pin) {
    return (1UL << pin);
}

/**
 * @brief Maps mHAL GPIO mode to hardware-specific GPIO mode
 *
 * @param [in] mode   mHAL GPIO mode
 *
 * @return  equivalent hardware-specific HAL GPIO mode
 */
static uint32_t GetGpioMode(uint32_t mode) {
    uint32_t retval = 0UL;

    switch (mode) {
    case GPIO_MODE_IN:
        retval = GPIO_MODE_INPUT;
        break;
    case GPIO_MODE_OUT_PP:
        retval = GPIO_MODE_OUTPUT_PP;
        break;
    case GPIO_MODE_OUT_OD:
        retval = GPIO_MODE_OUTPUT_OD;
        break;
    case GPIO_MODE_ALT_PP:
        retval = GPIO_MODE_AF_PP;
        break;
    case GPIO_MODE_ALT_OD:
        retval= GPIO_MODE_AF_OD;
        break;
    case GPIO_MODE_ANLG:
        retval = GPIO_MODE_ANALOG;
        break;
    case GPIO_MODE_INTR_RISING:
        retval = GPIO_MODE_IT_RISING;
        break;
    case GPIO_MODE_INTR_FALLING:
        retval = GPIO_MODE_IT_FALLING;
        break;
    case GPIO_MODE_INTR_EDGE:
        retval = GPIO_MODE_IT_RISING_FALLING;
        break;
    case GPIO_MODE_EVENT_RISING:
        retval = GPIO_MODE_EVT_RISING;
        break;
    case GPIO_MODE_EVENT_FALLING:
        retval = GPIO_MODE_EVT_FALLING;
        break;
    case GPIO_MODE_EVENT_EDGE:
        retval = GPIO_MODE_EVT_RISING_FALLING;
        break;
    default:
        break;

    }

    return retval;
}

/**
 * @brief Maps mHAL GPIO pull mode to hardware-specific GPIO pull mode
 *
 * @param [in] pull   mHAL GPIO pull mode
 *
 * @return  equivalent hardware-specific GPIO pull mode
 */
static uint32_t GetGpioPull(GpioPull pull) {
    uint32_t retval = 0UL;

    switch (pull) {
    case GPIO_PULL_NONE:
        retval = GPIO_NOPULL;
        break;
    case GPIO_PULL_UP:
        retval = GPIO_PULLUP;
        break;
    case GPIO_PULL_DOWN:
        retval = GPIO_PULLDOWN;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Maps mHAL GPIO speed to hardware-specific GPIO speed
 *
 * @param [in] speed   mHAL GPIO speed
 *
 * @return  equivalent hardware-specific GPIO speed
 */
static uint32_t GetGpioSpeed(GpioSpeed speed) {
    uint32_t retval = 0UL;

    switch (speed) {
    case GPIO_SPEED_LO:
        retval = GPIO_SPEED_FREQ_LOW;
        break;
    case GPIO_SPEED_MED:
        retval = GPIO_SPEED_FREQ_MEDIUM;
        break;
    case GPIO_SPEED_HI:
        retval = GPIO_SPEED_FREQ_HIGH;
        break;
#if defined(STM32CubeG4)
    case GPIO_SPEED_VERY_HI:
        retval = GPIO_SPEED_FREQ_VERY_HIGH;
        break;
#endif // STM32CubeG4
    default:
        break;
    }

    return retval;
}

/**
 * @brief Enable GPIO clock
 *
 * @param [in] port   mHAL GPIO port
 *
 * @todo enable GPIOG and GPIOH when we have an MCU that supports them
 */
static void GpioClockEnable(GpioPort port) {
    switch (port) {
    case GPIO_PORT_A:
        __HAL_RCC_GPIOA_CLK_ENABLE();
        break;
    case GPIO_PORT_B:
        __HAL_RCC_GPIOB_CLK_ENABLE();
        break;
    case GPIO_PORT_C:
        __HAL_RCC_GPIOC_CLK_ENABLE();
        break;
    case GPIO_PORT_D:
        __HAL_RCC_GPIOD_CLK_ENABLE();
        break;
    case GPIO_PORT_E:
        __HAL_RCC_GPIOE_CLK_ENABLE();
        break;
    case GPIO_PORT_F:
        __HAL_RCC_GPIOF_CLK_ENABLE();
        break;
#if defined(STM32CubeG4)
    case GPIO_PORT_G:
        __HAL_RCC_GPIOG_CLK_ENABLE();
        break;
#endif // STM32CubeG4
    default:
        break;
    }
}

mHalStatus HalGpio_Init(Gpio* pGpio, GpioConfig* config) {
    if (!pGpio || !config) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_TypeDef* gpio_port = GetGpioPort(config->port);
    if (!gpio_port) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    GpioClockEnable(config->port);

    GPIO_InitStruct.Pin       = GetGpioPin(config->pin);
    GPIO_InitStruct.Mode      = GetGpioMode(config->mode);
    GPIO_InitStruct.Pull      = GetGpioPull(config->pull);
    GPIO_InitStruct.Speed     = GetGpioSpeed(config->speed);
    GPIO_InitStruct.Alternate = config->alt_func;
    HAL_GPIO_Init(gpio_port, &GPIO_InitStruct);
    return MHAL_STATUS_OK;
}

mHalStatus HalGpio_EnableExtInterrupt(Gpio gpio,
    mHalInterruptPriority priority,
    mHalGpio_ExternalInterruptCb callback, void* ctx) {
    if (!gpio) {
        return HAL_STATUS_ERROR_INSTANCE;
    }

    // save callback and context
    exti_cb_[pin].callback = callback;
    exti_cb_[pin].ctx      = ctx;

    // Enable/Disable interrupt
    IRQn_Type irqn = GetExtiIRQn(pin);
    if (callback == NULL) {
        HAL_NVIC_DisableIRQ(irqn);
    }
    else {
        HAL_NVIC_SetPriority(irqn, priority, INTERRUPT_SUBPRIORITY_NOT_USED);
        HAL_NVIC_EnableIRQ(irqn);
    }
    return HAL_STATUS_OK;
}

mHalStatus HalGpio_Write(Gpio gpio, GpioState state) {
    if (!gpio) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    GPIO_TypeDef* gpio_port = GetGpioPort(gpio->port);
    if (!gpio_port) {
        return MHAL_STATUS_ERROR;
    }

    uint32_t gpio_pin = GetGpioPin(gpio->pin);

    HAL_GPIO_WritePin(gpio->port, gpio->pin, (GPIO_PinState) state);
    return MHAL_STATUS_OK;
}

mHalStatus HalGpio_Read(Gpio gpio, GpioState *state) {
    if (!gpio || !state) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    GPIO_TypeDef* gpio_port = GetGpioPort(gpio->port);
    if (!gpio_port) {
        return MHAL_STATUS_ERROR;
    }

    uint32_t gpio_pin = GetGpioPin(gpio->pin);

    *state = ((GpioState) HAL_GPIO_ReadPin(gpio_port, gpio_pin));
    return MHAL_STATUS_OK;
}

mHalStatus HalGpio_Toggle(Gpio gpio) {
    if (!gpio) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    GPIO_TypeDef* gpio_port = GetGpioPort(gpio->port);
    if (!gpio_port) {
        return MHAL_STATUS_ERROR;
    }

    uint32_t gpio_pin = GetGpioPin(gpio->pin);

    HAL_GPIO_TogglePin(gpio_port, gpio_pin);
    return MHAL_STATUS_OK;
}

/**
  * @brief STM32Cube external interrupt callback override
  *
  * @param [in] bit  external interrupt bit set
  */
void HAL_GPIO_EXTI_Callback(uint16_t bit) {
    size_t pin = 31 - __CLZ(bit);
    if ((pin < NUM_GPIO_PIN) && (exti_cb_[pin].callback != NULL)) {
        exti_cb_[pin].callback(exti_cb_[pin].ctx);
    }
}

/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief Hardware interrupt handler for EXTI0
  */
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BIT(0));
}

/**
  * @brief Hardware interrupt handler for EXTI1
  */
void EXTI1_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BIT(1));
}

/**
  * @brief Hardware interrupt handler for EXTI2
  */
void EXTI2_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BIT(2));
}

/**
  * @brief Hardware interrupt handler for EXTI3
  */
void EXTI3_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BIT(3));
}

/**
  * @brief Hardware interrupt handler for EXTI4
  */
void EXTI4_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(BIT(4));
}

/**
  * @brief Hardware interrupt handler for EXTI5 to EXTI9
  */
void EXTI9_5_IRQHandler(void) {
    // HAL_GPIO_EXTI_IRQHandler() only handles interrupt if flag is set
    HAL_GPIO_EXTI_IRQHandler(BIT(5));
    HAL_GPIO_EXTI_IRQHandler(BIT(6));
    HAL_GPIO_EXTI_IRQHandler(BIT(7));
    HAL_GPIO_EXTI_IRQHandler(BIT(8));
    HAL_GPIO_EXTI_IRQHandler(BIT(9));
}

/**
  * @brief Hardware interrupt handler for EXTI10 to EXTI15
  */
void EXTI15_10_IRQHandler(void) {
    // HAL_GPIO_EXTI_IRQHandler() only handles interrupt if flag is set
    HAL_GPIO_EXTI_IRQHandler(BIT(10));
    HAL_GPIO_EXTI_IRQHandler(BIT(11));
    HAL_GPIO_EXTI_IRQHandler(BIT(12));
    HAL_GPIO_EXTI_IRQHandler(BIT(13));
    HAL_GPIO_EXTI_IRQHandler(BIT(14));
    HAL_GPIO_EXTI_IRQHandler(BIT(15));
}
