/**
 * @file ring_buffer.c
 * 
 * @brief Ring buffer implementation to support monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "ring_buffer.h"

typedef struct _ring_buffer {
    uint8_t* buffer;
    uint32_t size;
    uint32_t size_mask;
    uint32_t head;
    uint32_t tail;
} ring_buffer;

/**
  * @brief  Check if a given value is a power of 2.
  * 
  * @param  num  number to check
  * 
  * @retval true if num is a power of 2, otherwise false
  */
static bool IsPowerOfTwo(uint32_t num) {
    return (num && ((num & (num - 1)) == 0));
}

mHalStatus RingBuffer_Create(RingBuffer* pBuffer, size_t size) {
    if (!size || !IsPowerOfTwo(size)) {
        *pBuffer = NULL;
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    RingBuffer buffer = malloc(sizeof(ring_buffer));
    if (!buffer) {
        *pBuffer = NULL;
        return MHAL_STATUS_ERROR;
    }
    buffer->buffer = malloc(size);
    if (!buffer->buffer) {
        free(buffer);
        *pBuffer = NULL;
        return MHAL_STATUS_ERROR;
    }
    buffer->size = size;
    buffer->size_mask = size - 1;
    buffer->head = 0;
    buffer->tail = 0;

    *pBuffer = buffer;

    return MHAL_STATUS_OK;
}

mHalStatus RingBuffer_Destroy(RingBuffer* pBuffer) {
    if(!pBuffer) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    free((*pBuffer)->buffer);
    free(*pBuffer);
    *pBuffer = NULL;

    return MHAL_STATUS_OK;
}

bool RingBuffer_IsEmpty(RingBuffer buffer) {
	if (buffer) {
        return buffer->head == buffer->tail;
	}
	return true;
}

mHalStatus RingBuffer_AddData(RingBuffer buffer, const uint8_t* data, size_t size) {
    if (buffer && data && size && (size <= buffer->size)) {
        for (size_t i = 0; i < size; ++i) {
            buffer->buffer[buffer->tail] = data[i];
            ++buffer->tail;
            buffer->tail &= buffer->size_mask;
        }
        return MHAL_STATUS_OK;
    }
    return MHAL_STATUS_ERROR_INVALID_PARAM;
}

mHalStatus RingBuffer_GetData(RingBuffer buffer, uint8_t* data, size_t size) {
    if (buffer && data && size && (size <= buffer->size) && (buffer->head != buffer->tail)) {
        for (size_t i = 0; i < size; ++i) {
    	    data[i] = buffer->buffer[buffer->head];
            ++buffer->head;
            buffer->head &= buffer->size_mask;
        }
        return MHAL_STATUS_OK;
	}
    return MHAL_STATUS_ERROR_INVALID_PARAM;
}
