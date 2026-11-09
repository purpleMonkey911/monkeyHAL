/**
 * @file mhal_misc.c
 * 
 * @brief Miscellaneous HAL functions for monkeyHAL. 
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <stdio.h>
#include <string.h>

#include "mhal_misc.h"

#define CPU_INFO_FILE "/proc/cpuinfo"
#define BUFFER_SZ_BYTES 80


void mHalMisc_GetUniqueId(uint8_t* buffer, size_t buffer_size) {
    if (!buffer || !buffer_size) {
        return;
    }

    FILE* fp = fopen(CPU_INFO_FILE, "r");
    if (!fp) {
        return;
    }

    memset(buffer, 0, buffer_size);
    char temp_buffer[BUFFER_SZ_BYTES];
    while (fgets(temp_buffer, sizeof(temp_buffer), fp)) {
        if (strncmp(temp_buffer, "Serial", 6) == 0) {
            sscanf(temp_buffer, "Serial : %lx", (uint64_t*) buffer); 
            break;
        }
    }

    fclose(fp);
}
