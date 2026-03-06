/**
 * @file hal_interrupt.c
 * 
 * @brief Interrupt priority levels.
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#include <stdint.h>

#include "mhal_interrupt.h"

#define ARM_CORTEX_CM4_NUM_INTR_REGS    8

/**
 * @brief Clear all interrupts
 * 
 */
void mHalIntr_Clear(void) {
    // Clear all 8 NVIC pending registers (covers up to 256 interrupts)
    for (uint8_t i = 0; i < ARM_CORTEX_CM4_NUM_INTR_REGS; i++) {
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }    
}
