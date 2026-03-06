/**
 * @file queue.c
 *
 * @brief Queue implementation to support mHAL.
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "queue.h"


struct _queue_t {
    uint8_t* buffer;          // memory for storing all elements in the queue
    size_t   element_size;    // element size in bytes
    size_t   max_elements;    // max number of elements in queue
    uint32_t read_index;      // number of reads from queue (index = read_index % max_elements)
    uint32_t write_index;     // number of writes to queue (index = write_index % max_elements)
} queue_t;

mHalStatus Queue_Create(Queue* pQueue, size_t element_size, size_t max_elements) {
    if (!pQueue || !element_size || !max_elements) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    Queue queue = malloc(sizeof(queue_t));
    if (!queue) {
        *pQueue = NULL;
        return MHAL_STATUS_ERROR;
    }

    queue->buffer = malloc(element_size * max_elements);
    if (!queue->buffer) {
        free(queue);
        *pQueue = NULL;
        return MHAL_STATUS_ERROR;
    }

    queue->element_size = element_size;
    queue->max_elements = max_elements;
    queue->read_index = 0;
    queue->write_index = 0;

    *pQueue = queue;
    return MHAL_STATUS_OK;
}

mHalStatus Queue_Destroy(Queue* pQueue) {
    if(!pQueue) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    free((*pQueue)->buffer);
    free(*pQueue);
    *pQueue = NULL;

    return MHAL_STATUS_OK;
}

bool Queue_IsEmpty(Queue queue) {
	if (queue) {
        return queue->read_index == queue->write_index;
	}
	return false;
}

uint32_t Queue_Count(Queue queue) {
    return (queue->write_index - queue->read_index);
}

mHalStatus Queue_Enqueue(Queue queue, const void* element) {
    if (!queue || !element) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // If Queue is full then return error
    if (Queue_Count(queue) >= queue->max_elements) {
        return MHAL_STATUS_ERROR;
    }

    uint32_t index = queue->write_index % queue->max_elements;
    uint8_t* ptr = &queue->buffer[index * queue->element_size];
    memcpy(ptr, element, queue->element_size);
    queue->write_index++;
    return MHAL_STATUS_OK;
}

mHalStatus Queue_Dequeue(Queue queue, void* element) {
    if (!queue || !element) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    if (Queue_IsEmpty(queue)) {
        return MHAL_STATUS_ERROR;
    }

    uint32_t index = queue->read_index % queue->max_elements;
    uint8_t* ptr = &queue->buffer[index * queue->element_size];
    memcpy(element, ptr, queue->element_size);
    queue->read_index++;
    return MHAL_STATUS_OK;
}
