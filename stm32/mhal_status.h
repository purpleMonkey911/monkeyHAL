/**
 * @file mhal_status.h
 * @brief Status return on HAL operations
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __MHAL_STATUS_H__
#define __MHAL_STATUS_H__

typedef enum {
    MHAL_STATUS_ERROR_IO            = -6,
    MHAL_STATUS_ERROR_INVALID_PARAM = -5,
    MHAL_STATUS_ERROR_DISABLED      = -4,
    MHAL_STATUS_ERROR_INSTANCE      = -3,
    MHAL_STATUS_ERROR_TIMEOUT       = -2,
    MHAL_STATUS_ERROR               = -1,
    MHAL_STATUS_OK                  = 0,
    MHAL_STATUS_BUSY                = 1,
    MHAL_STATUS_DONE                = 2,
    MHAL_STATUS_DATA_INVALID        = 3,
} mHalStatus;

#endif // __MHAL_STATUS_H__
