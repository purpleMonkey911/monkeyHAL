/**
 * @file mhal_misc.h
 * 
 * @brief Miscellaneous HAL functions for monkeyHAL. 
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 */

#ifndef __MHAL_MISC_H__
#define __MHAL_MISC_H__

#include <stdint.h>
#include <stddef.h>

/**
 * @brief Returns Unique Device ID.
 *        The Unique Device ID is specific to each chip and written by the 
 *        manufacturer.  This is a read-only value
 *
 * @param buffer      the buffer to hold the unique ID
 * @param buffer_size the size (in bytes) of buffer
 */
extern void mHalMisc_GetUniqueId(uint8_t* buffer, size_t buffer_size);

#endif // __MHAL_MISC_H__
