/**
 * @file _hal_internal.h
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_INTERNAL_H__
#define __MHAL_INTERNAL_H__

#ifndef _UNITTEST
  #if defined(STM32CubeF3)
    #include "stm32f3xx_hal_def.h"
  #elif defined(STM32CubeG4)
    #include "stm32g4xx_hal_def.h"
  #elif defined(STM32CubeL4)
    #include "stm32l4xx_hal_def.h"
  #else
    #error "MCU not supported!"
  #endif 
#else
  #include "mttr-hal-mocks.h"
#endif // _UNITTEST

#include "mhal_status.h"
#include "hal_interrupt.h"

// Macro used to validate device argument
#define _HAL_VALIDATE_DEV(pDev, enabled)                \
    do {                                                \
        if (!pDev)    return HAL_STATUS_ERROR_INSTANCE; \
        if (!enabled) return HAL_STATUS_ERROR_DISABLED; \
    } while (0)


// Macro used to return status code on error of calling HAL functions
#define _HAL_RETURN_ON_ERROR(result)                    \
    do {                                                \
        HalStatus status = (result);                    \
        if (status != HAL_STATUS_OK) return status;     \
    } while (0)


#define BIT(x)   (1<<(x))

// NVIC_PRIORITYGROUP_4 is configured so sub-priority levels are not used
#define INTERRUPT_SUBPRIORITY_NOT_USED  0

extern mHalStatus _mhal_translateStatus(HAL_StatusTypeDef status);
extern void _mhal_checkInterruptPriority(IRQn_Type irq);

#endif // __HAL_INTERNAL_H__
