/**
 * @file mhal_can.h
 * 
 * @brief CAN HAL implementation for monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __MHAL_CAN_H__
#define __MHAL_CAN_H__

#include <linux/can.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "mhal_status.h"


typedef struct _can_t can_t;
typedef can_t* Can;

typedef enum {
    // NOTE: CAN v1 is NOT supported! 
    CAN_VER_2_0_A,
    CAN_VER_2_0_B,
} CanVersion;

typedef enum {
    CAN_DLC_BYTES_INVALID = -1,
    CAN_DLC_BYTES_0       = 0,
    CAN_DLC_BYTES_1       = 1,
    CAN_DLC_BYTES_2       = 2,
    CAN_DLC_BYTES_3       = 3,
    CAN_DLC_BYTES_4       = 4,
    CAN_DLC_BYTES_5       = 5,
    CAN_DLC_BYTES_6       = 6,
    CAN_DLC_BYTES_7       = 7,
    CAN_DLC_BYTES_8       = 8,
    CAN_DLC_BYTES_12      = 9,
    CAN_DLC_BYTES_16      = 10,
    CAN_DLC_BYTES_20      = 11,
    CAN_DLC_BYTES_24      = 12,
    CAN_DLC_BYTES_32      = 13,
    CAN_DLC_BYTES_48      = 14,
    CAN_DLC_BYTES_64      = 15,
} CanDlc;  // Data Length Code

#define CAN_INTERFACE_NAME_MAX_LENGTH 32
typedef struct {
    char name[CAN_INTERFACE_NAME_MAX_LENGTH]; // Name of the CAN interface (e.g., "can0")
    uint32_t bitrate;                         // Bitrate in bits per second
    CanVersion can_version;                   // Version of the CAN protocol
    bool fd_support;                          // Whether CAN FD is supported
    size_t queue_size;                        // Size of the queue for CAN frames
    size_t socket_buffer_size;                // socket buffer size for CAN frames
    size_t socket_txqueuelen;                 // socket queue size for CAN frames
} CanConfig;

/**
 * @brief Initialize Can instance.  This function sets all the CAN parameters
 *        and sets up the I/O interrupts.
 * 
 * @param index            index of which CAN to initialize
 * @param msg_id           CAN message ID
 * @param can_version      CAN version as per CAN ICD
 * @param fd_support       true if this is CAN bus will support FDCAN
 * @param ring_buffer_size size of the RingBuffer
 * 
 * @return MHAL_STATUS_OK if successfully initialized, otherwise error state
 */
extern mHalStatus mHalCan_Init(Can* pCan, const CanConfig* config);

/**
 * @brief Deinitialize Can instance. This function closes the CAN socket,
 *        destroys the queues, and frees the allocated memory for the Can instance.
 * 
 * @param pCan Pointer to the Can instance to deinitialize
 * 
 * @return MHAL_STATUS_OK if successful, otherwise error state
 */
extern mHalStatus mHalCan_Deinit(Can* pCan);

/**
 * @brief Get data size from CAN DLC
 * 
 * @param can_dlc CAN DLC value
 * 
 * @return data size
 */
extern size_t mHalCan_GetSizeFromDlc(CanDlc can_dlc);

/**
 * @brief Get CAN DLC from data size
 * 
 * @param size data size
 * 
 * @return CAN DLC value
 */
extern CanDlc mHalCan_GetDlcFromSize(size_t size);

/**
 * @brief Send a CAN frame through the CAN interface.
 *        Techinically, this function enqueues the frame to the TX queue.  The
 *        Can state machine will handle the actual transmission.
 * 
 * @param can Pointer to the Can instance
 * @param frame Pointer to the CAN frame to send
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalCan_SendCanFrame(Can can, struct can_frame* frame);

/**
 * @brief Get a CAN frame from the RX queue.
 *        Technically, this function dequeues the frame from the RX queue.  The
 *        Can state machine will handle the actual reception.
 * 
 * @param can Pointer to the Can instance
 * @param frame Pointer to the structure to store the received CAN frame
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalCan_GetCanFrame(Can can, struct can_frame* frame);

/**
 * @brief CAN state machine (see file comments for detailed description)
 * 
 * @param can Pointer to the Can instance
 * 
 * @return MHAL_STATUS_OK if state machine executed properly, otherwise a monkeyHAL error
 */
extern mHalStatus mHalCan_StateMachine(Can can);

#endif // __MHAL_CAN_H__
