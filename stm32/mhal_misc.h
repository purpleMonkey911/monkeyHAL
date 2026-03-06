/**
 * @file hal_misc.h
 * 
 * @brief Miscellaneous abstraction layer for mHAL.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 */

#ifndef __MHAL_MISC_H__
#define __MHAL_MISC_H__

#include <stdint.h>
#include <stddef.h>

extern void mHalMisc_GetUniqueId(uint8_t* buffer, size_t buffer_size);

#endif // __MHAL_MISC_H__
