/**
 * @file mhal_dma.c
 * 
 * @brief DMA Hardware Abstraction Layer for mHAL
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#if defined(STM32CubeF3)
  #include "stm32f3xx_hal.h"
#elif defined(STM32CubeG4)
  #include "stm32g4xx_hal.h"
#elif defined (STM32CubeL4)
  #include "stm32l4xx_hal.h"
#else
  #error "MCU not supported!"
#endif

#include <stddef.h>

#include "_mhal_internal.h"
#include "mhal_dma_internal.h"
#include "mhal_dma.h"

// Invalid register value used for indicating setup error
#define INVALID_REG_VALUE  0xFFFFFFFF

// An alias for internal DMA structure DMA_HandleTypeDef
typedef DMA_HandleTypeDef _dma_t;

typedef struct {
    DMA_HandleTypeDef hDma;  // STM32Cube DMA handle
} DmaChannel;

static DmaChannel dma_inst[NUM_DMA_CHANNELS] = {0};


/**
 * @brief Get hardware-specific DMA peripheral.
 *
 * @param [in] index  index for which DMA will retrieve
 *
 * @return pointer to DMA peripheral
 */
static DMA_Channel_TypeDef* GetChannelBase(DmaIndex index) {
    switch (index) {
        case DMA1_CHANNEL_1:  return DMA1_Channel1;
        case DMA1_CHANNEL_2:  return DMA1_Channel2;
        case DMA1_CHANNEL_3:  return DMA1_Channel3;
        case DMA1_CHANNEL_4:  return DMA1_Channel4;
        case DMA1_CHANNEL_5:  return DMA1_Channel5;
        case DMA1_CHANNEL_6:  return DMA1_Channel6;
        case DMA1_CHANNEL_7:  return DMA1_Channel7;
#ifdef STM32G474xx
        case DMA1_CHANNEL_8:  return DMA1_Channel8;
#endif // STM32G474xx
        case DMA2_CHANNEL_1:  return DMA2_Channel1;
        case DMA2_CHANNEL_2:  return DMA2_Channel2;
        case DMA2_CHANNEL_3:  return DMA2_Channel3;
        case DMA2_CHANNEL_4:  return DMA2_Channel4;
        case DMA2_CHANNEL_5:  return DMA2_Channel5;
#ifndef STM32F303xC
        case DMA2_CHANNEL_6:  return DMA2_Channel6;
        case DMA2_CHANNEL_7:  return DMA2_Channel7;
#ifdef STM32G474xx
        case DMA2_CHANNEL_8:  return DMA2_Channel8;
#endif // STM32G474xx
#endif // !STM32F303xC
        default:              return NULL;
    }
}

/**
 * @brief Get hardware-specific DMA direction.
 *
 * @param [in] dir  DMA direction enum value
 *
 * @return hardware-specific DMA direction
 */
static uint32_t GetDirection(DmaDirection dir) {
    switch (dir) {
        case DMA_DIR_PERIPH_TO_MEMORY: return DMA_PERIPH_TO_MEMORY;
        case DMA_DIR_MEMORY_TO_PERIPH: return DMA_MEMORY_TO_PERIPH;
        case DMA_DIR_MEMORY_TO_MEMORY: return DMA_MEMORY_TO_MEMORY;
        default:                       return INVALID_REG_VALUE;
    }
}

/**
 * @brief Get hardware-specific DMA peripheral data alignment.
 *
 * @param [in] alignment  DMA peripheral data alignment enum value
 *
 * @return hardware-specific DMA peripheral data alignment
 */
static uint32_t GetPeriphDataAlignment(DmaDataAlignment alignment) {
    switch (alignment) {
        case DMA_DATA_ALIGNMENT_BYTE:     return DMA_PDATAALIGN_BYTE;
        case DMA_DATA_ALIGNMENT_HALFWORD: return DMA_PDATAALIGN_HALFWORD;
        case DMA_DATA_ALIGNMENT_WORD:     return DMA_PDATAALIGN_WORD;
        default:                          return INVALID_REG_VALUE;
    }
}

/**
 * @brief Get hardware-specific DMA memory data alignment.
 *
 * @param [in] alignment  DMA memory data alignment enum value
 *
 * @return hardware-specific DMA memory data alignment
 */
static uint32_t GetMemoryDataAlignment(DmaDataAlignment alignment) {
    switch (alignment) {
        case DMA_DATA_ALIGNMENT_BYTE:     return DMA_MDATAALIGN_BYTE;
        case DMA_DATA_ALIGNMENT_HALFWORD: return DMA_MDATAALIGN_HALFWORD;
        case DMA_DATA_ALIGNMENT_WORD:     return DMA_MDATAALIGN_WORD;
        default:                          return INVALID_REG_VALUE;
    }
}

/**
 * @brief Get hardware-specific DMA priority.
 *
 * @param [in] priority  DMA priority enum value
 *
 * @return hardware-specific DMA priority
 */
static uint32_t GetDmaPriority(DmaPriority priority) {
    switch (priority) {
        case DMA_PRI_LOW:       return DMA_PRIORITY_LOW;
        case DMA_PRI_MEDIMUM:   return DMA_PRIORITY_MEDIUM;
        case DMA_PRI_HIGH:      return DMA_PRIORITY_HIGH;
        case DMA_PRI_VERY_HIGH: return DMA_PRIORITY_VERY_HIGH;
        default:                return INVALID_REG_VALUE;
    }
}

/**
 * @brief Enable DMA peripheral clock.
 *
 * @param [in] index  index of which DMA clock to enable
 */
static void DmaClockEnable(DmaIndex index) {
#ifdef STM32CubeG4
    // This is used by both DMA1 and DMA2
    __HAL_RCC_DMAMUX1_CLK_ENABLE();
#endif

    if (index < DMA2_CHANNEL_1) {
        __HAL_RCC_DMA1_CLK_ENABLE();
    }
    else if (index < NUM_DMA_CHANNELS) {
        __HAL_RCC_DMA2_CLK_ENABLE();
    }
}

/**
 * @brief Get DMA interrupt request vector.
 *
 * @param [in] index  index for which DMA will retrieve
 *
 * @return DMA interrupt request vector
 */
__attribute__((used))
static IRQn_Type GetDmaIRQn(DmaIndex index) {
    switch (index) {
        case DMA1_CHANNEL_1:  return DMA1_Channel1_IRQn;
        case DMA1_CHANNEL_2:  return DMA1_Channel2_IRQn;
        case DMA1_CHANNEL_3:  return DMA1_Channel3_IRQn;
        case DMA1_CHANNEL_4:  return DMA1_Channel4_IRQn;
        case DMA1_CHANNEL_5:  return DMA1_Channel5_IRQn;
        case DMA1_CHANNEL_6:  return DMA1_Channel6_IRQn;
        case DMA1_CHANNEL_7:  return DMA1_Channel7_IRQn;
#ifdef STM32G474xx
        case DMA1_CHANNEL_8:  return DMA1_Channel8_IRQn;
#endif // STM32G474xx
        case DMA2_CHANNEL_1:  return DMA2_Channel1_IRQn;
        case DMA2_CHANNEL_2:  return DMA2_Channel2_IRQn;
        case DMA2_CHANNEL_3:  return DMA2_Channel3_IRQn;
        case DMA2_CHANNEL_4:  return DMA2_Channel4_IRQn;
        case DMA2_CHANNEL_5:  return DMA2_Channel5_IRQn;
#ifndef STM32F303xC 
        case DMA2_CHANNEL_6:  return DMA2_Channel6_IRQn;
        case DMA2_CHANNEL_7:  return DMA2_Channel7_IRQn;
#ifdef STM32G474xx
        case DMA2_CHANNEL_8:  return DMA2_Channel8_IRQn;
#endif // STM32G474xx
#endif // !STM32F303xC
        default:              return 0;
    }
}

/**
 * @brief Get DMA instance.
 *
 * @param [in] index  index for which DMA will retrieve
 *
 * @return DmaChannel instance if index is valid, NULL otherwise
 */
static DmaChannel* GetDma(DmaIndex index) {
    if (index >= NUM_DMA_CHANNELS) {
        return NULL;
    }
    return &dma_inst[index];
}

/**
 * @brief Check if DMA is enabled.
 *
 * @param [in] pDma  pointer to DmaChannel structure
 *
 * @return true if enabled, otherwise false
 */
static bool IsDmaEnabled(DmaChannel* pDma) {
    if (!pDma) {
        return false;
    }
    return (pDma->hDma.Instance != NULL);
}

/**
 * @brief Get DmaChannel instance from STM32 DMA handle.
 *
 * @param [in] hDma  pointer to STM32 DMA handle
 *
 * @return DmaChannel instance if valid handle, else NULL
 */
__attribute__((used))
static DmaChannel* GetDmaFromHandle(DMA_HandleTypeDef* hDma) {
    DmaChannel* retval = NULL;

    for (uint8_t index = 0; index < NUM_DMA_CHANNELS; index++) {
        if (hDma == &dma_inst[index].hDma) {
            retval = &dma_inst[index];
            break;
        }
    }
    return retval;
}

mHalStatus HalDma_SetInterruptPriority(DmaIndex index, mHalInterruptPriority prior) {
    if (index >= NUM_DMA_CHANNELS) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }
    IRQn_Type irqn = GetDmaIRQn(index);
    HAL_NVIC_SetPriority(irqn, prior, INTERRUPT_SUBPRIORITY_NOT_USED);
    return MHAL_STATUS_OK;
}

mHalStatus HalDma_Init(DmaIndex index,
#ifdef STM32CubeG4
                      DmaRequestNum rqst_num,
#endif
                      DmaDirection dir,
                      bool pinc, bool minc,
                      DmaDataAlignment palign, DmaDataAlignment malign,
                      bool circular, DmaPriority priority) {

    if (index >= NUM_DMA_CHANNELS) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    DmaChannel* pDma = &dma_inst[index];
    pDma->hDma.Instance = GetChannelBase(index);
    pDma->hDma.Init.Direction = GetDirection(dir);
    pDma->hDma.Init.PeriphInc = (pinc) ? DMA_PINC_ENABLE : DMA_PINC_DISABLE;
    pDma->hDma.Init.MemInc = (minc) ? DMA_MINC_ENABLE : DMA_MINC_DISABLE;
    pDma->hDma.Init.PeriphDataAlignment = GetPeriphDataAlignment(palign);
    pDma->hDma.Init.MemDataAlignment = GetMemoryDataAlignment(malign);;
    pDma->hDma.Init.Mode = (circular) ? DMA_CIRCULAR : DMA_NORMAL;
    pDma->hDma.Init.Priority = GetDmaPriority(priority);

#ifdef STM32CubeG4
    pDma->hDma.Init.Request = rqst_num;
    if (pDma->hDma.Init.Request >= DMA_RQST_INVALID) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
#endif

    if (pDma->hDma.Instance == NULL ||
        pDma->hDma.Init.Direction == INVALID_REG_VALUE ||
        pDma->hDma.Init.PeriphDataAlignment == INVALID_REG_VALUE ||
        pDma->hDma.Init.MemDataAlignment == INVALID_REG_VALUE ||
        pDma->hDma.Init.Priority == INVALID_REG_VALUE) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    DmaClockEnable(index);
    if (HAL_DMA_Init(&pDma->hDma) != HAL_OK) {
        return MHAL_STATUS_ERROR;
    }

    // prioritize and enable interrupt
    IRQn_Type irqn = GetDmaIRQn(index);
    _mhal_checkInterruptPriority(irqn);
    HAL_NVIC_EnableIRQ(irqn);
    return MHAL_STATUS_OK;
}

/**
 * @brief Get internal STM32Cube handle for a DMA channel.
 *
 * @param [in] index  index for which DMA to retrieve
 *
 * @return pointer to abstract DMA channel structure
 */
dma_t* HalDma_GetHandle(DmaIndex index) {
    DmaChannel* dma = GetDma(index);
    if (IsDmaEnabled(dma)) {
        return (dma_t*)&dma->hDma;
    }
    return NULL;
}

/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief Hardware interrupt handler for DMA1 Channel1
  */
void DMA1_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_1].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel2
  */
void DMA1_Channel2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_2].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel3
  */
void DMA1_Channel3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_3].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel4
  */
void DMA1_Channel4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_4].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel5
  */
void DMA1_Channel5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_5].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel6
  */
void DMA1_Channel6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_6].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA1 Channel7
  */
void DMA1_Channel7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_7].hDma);
}

#ifdef STM32CubeG4
/**
  * @brief Hardware interrupt handler for DMA1 Channel8
  */
void DMA1_Channel8_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA1_CHANNEL_8].hDma);
}
#endif // #ifdef STM32CubeG4

/**
  * @brief Hardware interrupt handler for DMA2 Channel1
  */
void DMA2_Channel1_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_1].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel2
  */
void DMA2_Channel2_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_2].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel3
  */
void DMA2_Channel3_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_3].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel4
  */
void DMA2_Channel4_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_4].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel5
  */
void DMA2_Channel5_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_5].hDma);
}

#ifdef STM32CubeG4
/**
  * @brief Hardware interrupt handler for DMA2 Channel6
  */
void DMA2_Channel6_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_6].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel7
  */
void DMA2_Channel7_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_7].hDma);
}

/**
  * @brief Hardware interrupt handler for DMA2 Channel8
  */
void DMA2_Channel8_IRQHandler(void) {
    HAL_DMA_IRQHandler(&dma_inst[DMA2_CHANNEL_8].hDma);
}
#endif // #ifdef STM32CubeG4
