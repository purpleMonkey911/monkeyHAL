/**
 * @file _mhal_internal.c
 *
 * @brief Internal functions used by the monkeyHAL
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
#endif // _UNITTEST

#include "_mhal_internal.h"

/**
 * @brief Translates STM32Cube status code to Matternet's status code.
 *
 * @param [in] status  STM32Cube status code
 *
 * @return HalStatus  Matternet's equivalent status code
 */
mHalStatus _mhal_translateStatus(HAL_StatusTypeDef status) {
    switch (status) {
        case HAL_OK:      return MHAL_STATUS_OK;
        case HAL_BUSY:    return MHAL_STATUS_BUSY;
        case HAL_TIMEOUT: return MHAL_STATUS_ERROR_TIMEOUT;
        case HAL_ERROR:   // Intentional fallthrough
        default:          return MHAL_STATUS_ERROR;
    }
}

/**
 * @brief Checks the interrupt priority is set; if not set it to a default priority level.
 *        This function should be called in the peripheral init function only (e.g. HalUart_Init).
 *        An advanced user can change the priority level to be higher than the SysTick
 *        after the Init function if so desired.
 *
 * @param [in] irq  interrupt request number
 */
void _mhal_checkInterruptPriority(IRQn_Type irq) {
    uint32_t preempt_prior;
    uint32_t sub_prior;
    HAL_NVIC_GetPriority(irq, NVIC_PRIORITYGROUP_4, &preempt_prior, &sub_prior);

    // The peripheral interrupts should be lower priority than the SysTick interrupt
    // to avoid any locking issue related to timeout logic in the STM32Cube HAL.
    if (preempt_prior <= TICK_INT_PRIORITY) {
        HAL_NVIC_SetPriority(irq, HAL_INTERRUPT_PRIORITY_DEFAULT, INTERRUPT_SUBPRIORITY_NOT_USED);
    }
}
