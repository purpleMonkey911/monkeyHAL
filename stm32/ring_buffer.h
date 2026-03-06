/**
 * @file ring_buffer.h
 * 
 * @brief Ring buffer implementation to support monkeyHAL 
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __RING_BUFFER_H__
#define __RING_BUFFER_H__

#include "mhal_status.h"

typedef struct _ring_buffer ring_buffer;
typedef ring_buffer* RingBuffer;

/**
  * @brief  Create and allocate a RingBuffer type. 
  * 
  * @param  [in,out] pBuffer pointer to RingBuffer instance
  * @param  [in]     size    size of the buffer (must be a power of 2)
  * 
  * @return MHAL_STATUS_OK if successful, otherwise mHAL error
  * 
  * @warning This module uses dynamic memory allocation.
  */
extern mHalStatus RingBuffer_Create(RingBuffer* pBuffer, size_t size);

/**
  * @brief  Destroy and deallocate a RingBuffer type. 
  * 
  * @param  [in,out] pBuffer pointer to RingBuffer instance
  * 
  * @return MHAL_STATUS_OK if successful, otherwise a MHA error
  */
extern mHalStatus RingBuffer_Destroy(RingBuffer* pBuffer);

/**
  * @brief  Check if RingBuffer is empty. 
  * 
  * @param  [in] buffer  RingBuffer instance
  * 
  * @return true if RingBuffer is empty
  */
extern bool RingBuffer_IsEmpty(RingBuffer buffer);

/**
  * @brief  Add data to RingBuffer. 
  * 
  * @param  [in] buffer RingBuffer to add data to
  * @param  [in] data   data buffer containing to add
  * @param  [in] size   size of the data to add
  * 
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  */
extern mHalStatus RingBuffer_AddData(RingBuffer buffer, const uint8_t* data, size_t size);

/**
  * @brief  Get data from RingBuffer. 
  * 
  * @param  [in] buffer RingBuffer to get data from
  * @param  [out] data   data buffer to contain data to get
  * @param  [in] size   size of the data to retrieve
  * 
  * @return MHAL_STATUS_OK if successful, otherwise a mHAL error
  */
extern mHalStatus RingBuffer_GetData(RingBuffer buffer, uint8_t* data, size_t size);

#endif // __RING_BUFFER_H__
