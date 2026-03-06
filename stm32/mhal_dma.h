/**
 * @file mhal_dma.h
 * 
 * @brief  DMA Hardware Abstraction Layer for mHAL
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_DMA_H__
#define __MHAL_DMA_H__

#include <stdint.h>
#include <stdbool.h>
#include "mhal_status.h"
#include "mhal_interrupt.h"

#define DMA_PREEMPT_PRIORITY  0
#define DMA_SUBPRIORITY       1

typedef enum {
    DMA1_CHANNEL_1,
    DMA1_CHANNEL_2,
    DMA1_CHANNEL_3,
    DMA1_CHANNEL_4,
    DMA1_CHANNEL_5,
    DMA1_CHANNEL_6,
    DMA1_CHANNEL_7,
#ifdef STM32G474xx
    DMA1_CHANNEL_8,
#endif // STM32G474xx
    DMA2_CHANNEL_1,
    DMA2_CHANNEL_2,
    DMA2_CHANNEL_3,
    DMA2_CHANNEL_4,
    DMA2_CHANNEL_5,
#ifndef STM32F303xC
    DMA2_CHANNEL_6,
    DMA2_CHANNEL_7,
#ifdef STM32G474xx
    DMA2_CHANNEL_8,
#endif // STM32G474xx
#endif // !STM32F303xC
    NUM_DMA_CHANNELS
} DmaIndex;

typedef enum {
    DMA_DIR_PERIPH_TO_MEMORY,
    DMA_DIR_MEMORY_TO_PERIPH,
    DMA_DIR_MEMORY_TO_MEMORY,
} DmaDirection;

typedef enum {
    DMA_DATA_ALIGNMENT_BYTE,
    DMA_DATA_ALIGNMENT_HALFWORD,
    DMA_DATA_ALIGNMENT_WORD,
} DmaDataAlignment;

typedef enum {
    DMA_PRI_LOW,
    DMA_PRI_MEDIMUM,
    DMA_PRI_HIGH,
    DMA_PRI_VERY_HIGH,
} DmaPriority;

#ifdef STM32CubeG4
typedef enum {
    DMA_RQST_MEM2MEM            = 0U,
    DMA_RQST_GENERATOR0         = 1U,
    DMA_RQST_GENERATOR1         = 2U,
    DMA_RQST_GENERATOR2         = 3U,
    DMA_RQST_GENERATOR3         = 4U,
    DMA_RQST_ADC1               = 5U,
    DMA_RQST_DAC1_CHANNEL1      = 6U,
    DMA_RQST_DAC1_CHANNEL2      = 7U,
    DMA_RQST_TIM6_UP            = 8U,
    DMA_RQST_TIM7_UP            = 9U,
    DMA_RQST_SPI1_RX            = 10U,
    DMA_RQST_SPI1_TX            = 11U,
    DMA_RQST_SPI2_RX            = 12U,
    DMA_RQST_SPI2_TX            = 13U,
    DMA_RQST_SPI3_RX            = 14U,
    DMA_RQST_SPI3_TX            = 15U,
    DMA_RQST_I2C1_RX            = 16U,
    DMA_RQST_I2C1_TX            = 17U,
    DMA_RQST_I2C2_RX            = 18U,
    DMA_RQST_I2C2_TX            = 19U,
    DMA_RQST_I2C3_RX            = 20U,
    DMA_RQST_I2C3_TX            = 21U,
    DMA_RQST_I2C4_RX            = 22U,
    DMA_RQST_I2C4_TX            = 23U,
    DMA_RQST_USART1_RX          = 24U,
    DMA_RQST_USART1_TX          = 25U,
    DMA_RQST_USART2_RX          = 26U,
    DMA_RQST_USART2_TX          = 27U,
    DMA_RQST_USART3_RX          = 28U,
    DMA_RQST_USART3_TX          = 29U,
    DMA_RQST_UART4_RX           = 30U,
    DMA_RQST_UART4_TX           = 31U,
    DMA_RQST_UART5_RX           = 32U,
    DMA_RQST_UART5_TX           = 33U,
    DMA_RQST_LPUART1_RX         = 34U,
    DMA_RQST_LPUART1_TX         = 35U,
    DMA_RQST_ADC2               = 36U,
    DMA_RQST_ADC3               = 37U,
    DMA_RQST_ADC4               = 38U,
    DMA_RQST_ADC5               = 39U,
    DMA_RQST_QUADSPI            = 40U,
    DMA_RQST_DAC2_CHANNEL1      = 41U,
    DMA_RQST_TIM1_CH1           = 42U,
    DMA_RQST_TIM1_CH2           = 43U,
    DMA_RQST_TIM1_CH3           = 44U,
    DMA_RQST_TIM1_CH4           = 45U,
    DMA_RQST_TIM1_UP            = 46U,
    DMA_RQST_TIM1_TRIG          = 47U,
    DMA_RQST_TIM1_COM           = 48U,
    DMA_RQST_TIM8_CH1           = 49U,
    DMA_RQST_TIM8_CH2           = 50U,
    DMA_RQST_TIM8_CH3           = 51U,
    DMA_RQST_TIM8_CH4           = 52U,
    DMA_RQST_TIM8_UP            = 53U,
    DMA_RQST_TIM8_TRIG          = 54U,
    DMA_RQST_TIM8_COM           = 55U,
    DMA_RQST_TIM2_CH1           = 56U,
    DMA_RQST_TIM2_CH2           = 57U,
    DMA_RQST_TIM2_CH3           = 58U,
    DMA_RQST_TIM2_CH4           = 59U,
    DMA_RQST_TIM2_UP            = 60U,
    DMA_RQST_TIM3_CH1           = 61U,
    DMA_RQST_TIM3_CH2           = 62U,
    DMA_RQST_TIM3_CH3           = 63U,
    DMA_RQST_TIM3_CH4           = 64U,
    DMA_RQST_TIM3_UP            = 65U,
    DMA_RQST_TIM3_TRIG          = 66U,
    DMA_RQST_TIM4_CH1           = 67U,
    DMA_RQST_TIM4_CH2           = 68U,
    DMA_RQST_TIM4_CH3           = 69U,
    DMA_RQST_TIM4_CH4           = 70U,
    DMA_RQST_TIM4_UP            = 71U,
    DMA_RQST_TIM5_CH1           = 72U,
    DMA_RQST_TIM5_CH2           = 73U,
    DMA_RQST_TIM5_CH3           = 74U,
    DMA_RQST_TIM5_CH4           = 75U,
    DMA_RQST_TIM5_UP            = 76U,
    DMA_RQST_TIM5_TRIG          = 77U,
    DMA_RQST_TIM15_CH1          = 78U,
    DMA_RQST_TIM15_UP           = 79U,
    DMA_RQST_TIM15_TRIG         = 80U,
    DMA_RQST_TIM15_COM          = 81U,
    DMA_RQST_TIM16_CH1          = 82U,
    DMA_RQST_TIM16_UP           = 83U,
    DMA_RQST_TIM17_CH1          = 84U,
    DMA_RQST_TIM17_UP           = 85U,
    DMA_RQST_TIM20_CH1          = 86U,
    DMA_RQST_TIM20_CH2          = 87U,
    DMA_RQST_TIM20_CH3          = 88U,
    DMA_RQST_TIM20_CH4          = 89U,
    DMA_RQST_TIM20_UP           = 90U,
    DMA_RQST_AES_IN             = 91U,
    DMA_RQST_AES_OUT            = 92U,
    DMA_RQST_TIM20_TRIG         = 93U,
    DMA_RQST_TIM20_COM          = 94U,
    DMA_RQST_HRTIM1_M           = 95U,
    DMA_RQST_HRTIM1_A           = 96U,
    DMA_RQST_HRTIM1_B           = 97U,
    DMA_RQST_HRTIM1_C           = 98U,
    DMA_RQST_HRTIM1_D           = 99U,
    DMA_RQST_HRTIM1_E           = 100U,
    DMA_RQST_HRTIM1_F           = 101U,
    DMA_RQST_DAC3_CHANNEL1      = 102U,
    DMA_RQST_DAC3_CHANNEL2      = 103U,
    DMA_RQST_DAC4_CHANNEL1      = 104U,
    DMA_RQST_DAC4_CHANNEL2      = 105U,
    DMA_RQST_SPI4_RX            = 106U,
    DMA_RQST_SPI4_TX            = 107U,
    DMA_RQST_SAI1_A             = 108U,
    DMA_RQST_SAI1_B             = 109U,
    DMA_RQST_FMAC_READ          = 110U,
    DMA_RQST_FMAC_WRITE         = 111U,
    DMA_RQST_CORDIC_READ        = 112U,
    DMA_RQST_CORDIC_WRITE       = 113U,
    DMA_RQST_UCPD1_RX           = 114U,
    DMA_RQST_UCPD1_TX           = 115U,
    DMA_RQST_INVALID,
} DmaRequestNum;
#endif

/**
 * @brief Set interrupt priority.
 *
 * @param index  index of which DMA to set
 * @param prior  interrupt priority level
 *
 * @return HAL_STATUS_OK if successfully initialized, otherwise error state
 */
extern mHalStatus HalDma_SetInterruptPriority(DmaIndex index, mHalInterruptPriority prior);

/**
 * @brief Initialize DMA channel.
 *
 * @param [in] index     index for which DMA to initialize
 * @param [in] rqst_num  DMA peripheral request number (STM32CubeG4 Only)
 * @param [in] dir       DMA data direction
 * @param [in] pinc      enable peripheral data increment
 * @param [in] minc      enable memory data increment
 * @param [in] palign    peripheral data alignment
 * @param [in] malign    memory data alignment
 * @param [in] circular  true for circular mode, false for normal mode
 * @param [in] priority  DMA channel priority level
 *
 * @return HAL_STATUS_OK if successfully, otherwise error state
 */
#ifdef STM32CubeG4
extern mHalStatus HalDma_Init(DmaIndex index,
                             DmaRequestNum rqst_num,
                             DmaDirection dir, bool pinc, bool minc,
                             DmaDataAlignment palign, DmaDataAlignment malign,
                             bool circular, DmaPriority priority);
#else
extern mHalStatus HalDma_Init(DmaIndex index,
                             DmaDirection dir, bool pinc, bool minc,
                             DmaDataAlignment palign, DmaDataAlignment malign,
                             bool circular, DmaPriority priority);
#endif

#endif  // __MHAL_DMA_H__
