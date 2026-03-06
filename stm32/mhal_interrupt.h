/**
 * @file mhal_interrupt.h
 * 
 * @brief Interrupt priority levels.
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_INTERRUPT_H__
#define __MHAL_INTERRUPT_H__

#if defined(STM32CubeF3)
  #include "stm32f3xx_hal.h"
#elif defined(STM32CubeG4)
  #include "stm32g4xx_hal.h"
#elif defined(STM32CubeL4)
  #include "stm32l4xx_hal.h"
#else
  #error "MCU not supported!"
#endif

// The interrupt priority group is set to be NVIC_PRIORITYGROUP_4:
// 16 preempt priority levels and 0 sub-priority.
typedef enum {
    MHAL_INTERRUPT_PRIORITY_0  = 0,
    MHAL_INTERRUPT_PRIORITY_1  = 1,
    MHAL_INTERRUPT_PRIORITY_2  = 2,
    MHAL_INTERRUPT_PRIORITY_3  = 3,
    MHAL_INTERRUPT_PRIORITY_4  = 4,
    MHAL_INTERRUPT_PRIORITY_5  = 5,
    MHAL_INTERRUPT_PRIORITY_6  = 6,
    MHAL_INTERRUPT_PRIORITY_7  = 7,
    MHAL_INTERRUPT_PRIORITY_8  = 8,
    MHAL_INTERRUPT_PRIORITY_9  = 9,
    MHAL_INTERRUPT_PRIORITY_10 = 10,
    MHAL_INTERRUPT_PRIORITY_11 = 11,
    MHAL_INTERRUPT_PRIORITY_12 = 12,
    MHAL_INTERRUPT_PRIORITY_13 = 13,
    MHAL_INTERRUPT_PRIORITY_14 = 14,
    MHAL_INTERRUPT_PRIORITY_15 = 15,
    MHAL_INTERRUPT_PRIORITY_HIGHEST = MHAL_INTERRUPT_PRIORITY_0,
    MHAL_INTERRUPT_PRIORITY_LOWEST  = MHAL_INTERRUPT_PRIORITY_15,
} mHalInterruptPriority;

// Majority of the interrupt priorities should be lower priority than
// the system tick interrupt priority for the timeout logic to work.
#define MHAL_INTERRUPT_PRIORITY_DEFAULT   (TICK_INT_PRIORITY + 1)

// enable/disable/clear all interrupts
#define mHalIntr_Disable()  do { __disable_irq(); } while (0)
#define mHalIntr_Enable()  do { __enable_irq(); } while (0)
extern void mHalIntr_Clear(void);

// enable/disable/clear a specific interrupt
#define mHalIntr_DisableIRQ(IRQn) HAL_NVIC_DisableIRQ(IRQn)
#define mHalIntr_EnableIRQ(IRQn) HAL_NVIC_EnableIRQ(IRQn)
#define mHalIntr_ClearIRQ(IRQn) HAL_NVIC_ClearPendingIRQ(IRQn)


#endif // __MHAL_INTERRUPT_H__
