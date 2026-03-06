/**
 * @file hal_systick.c
 * 
 * @brief SysTick Abstraction Layer for mHAL)
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */


#if defined(STM32CubeF3)
  #include "stm32f3xx_hal.h"
#elif defined(STM32CubeG4)
  #include "stm32g4xx_hal.h"
#elif defined(STM32CubeL4)
  #include "stm32l4xx_hal.h"
#else
#error "MCU not supported!"
#endif

/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void) {
    HAL_IncTick();
}
