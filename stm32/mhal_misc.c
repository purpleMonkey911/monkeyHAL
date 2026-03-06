/**
 * @file hal_misc.c
 * 
 * @brief Miscellaneous abstraction layer for mHAL.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <stdint.h>
#include <string.h>

#if defined(STM32CubeF3)
#define UNIQUE_ID_ADDR 0x1FFFF7AC
#elif defined(STM32CubeG4) || defined(STM32CubeL4)
#define UNIQUE_ID_ADDR 0x1FFF7590
#else
#error MCU not supported
#endif

#define UNIQUE_ID_SIZE_UINT32 3  // 96-bits (3 x 32 bits)
#define UNIQUE_ID_SIZE_UINT8 12  // 96-bits (12 x 8 bits)
static const uint32_t* const unique_id_ptr = (uint32_t*) UNIQUE_ID_ADDR;

/**
 * @brief Returns Unique Device ID.
 *        The Unique Device ID is specific to each chip and written by the 
 *        manufacturer.  This is a read-only value
 *
 * @param [out] buffer      the buffer to hold the unique ID
 * @param [in]  buffer_size the size (in bytes) of buffer
 */
void mHalMisc_GetUniqueId(uint8_t* buffer, size_t buffer_size) {
    if (!buffer || !buffer_size) {
        return;
    }
    const uint32_t unique_id[UNIQUE_ID_SIZE_UINT32] = {
        unique_id_ptr[0],
        unique_id_ptr[1],
        unique_id_ptr[2],
    };

    memset(buffer, 0, buffer_size);
    memcpy(buffer, unique_id, (buffer_size > UNIQUE_ID_SIZE_UINT8) ?
        UNIQUE_ID_SIZE_UINT8 : buffer_size);
}
 