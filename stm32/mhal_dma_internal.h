/**
 * @file mhal_dma_internal.h
 * 
 * @brief DMA functions for internal use only.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __MHAL_DMA_INTERNAL_H__
#define __MHAL_DMA_INTERNAL_H__

#include "mhal_dma.h"

// Abstraction of DMA structure to be shared with other peripherals (e.g. Timer) via HalDma_GetHandle()
typedef struct _dma_t dma_t;

extern dma_t* HalDma_GetHandle(DmaIndex index);

#endif // __MHAL_DMA_INTERNAL_H__
