/**
 * @file queue.h
 * 
 * @brief Queue implementation to support monkeyHAL.
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 */

#ifndef __QUEUE_H__
#define __QUEUE_H__

#include "mhal_status.h"

typedef struct _queue_t queue_t;
typedef queue_t* Queue;

/**
  * @brief  Create and allocate a Queue type.
  *
  * @param  [in,out] pQueue        pointer to Queue instance
  * @param  [in]     element_size  element size in bytes
  * @param  [in]     max_elements  maximum number of elements
  *
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  *
  * @warning This module uses dynamic memory allocation.
  */
extern mHalStatus Queue_Create(Queue* pQueue, size_t element_size, size_t max_elements);

/**
  * @brief  Destroy and deallocate a Queue type.
  *
  * @param  [in,out] pQueue pointer to Queue instance
  *
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  */
extern mHalStatus Queue_Destroy(Queue* pQueue);

/**
  * @brief  Check if Queue is empty.
  *
  * @param  [in] queue  Queue instance
  *
  * @return true if Queue is empty, otherwise false
  */
extern bool Queue_IsEmpty(Queue queue);

/**
  * @brief  Get number of available elements in the queue.
  *
  * @param  [in] queue  Queue instance
  *
  * @return Number of elements available to be read
  */
extern uint32_t Queue_Count(Queue queue);

/**
  * @brief  Add element to the end of the Queue.
  *
  * @param  [in] queue    Queue instance to add element to
  * @param  [in] element  element to add
  *
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  */
extern mHalStatus Queue_Enqueue(Queue queue, const void* element);

/**
  * @brief  Get element from the front of the Queue.
  *
  * @param  [in] queue    Queue instanceto get element from
  * @param  [in] element  pointer to element to write to
  *
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  */
extern mHalStatus Queue_Dequeue(Queue queue, void* element);

#endif // __QUEUE_H__
