/**
 * @file mhal_can.c
 * 
 * @brief CAN HAL implementation for monkeyHAL.
 * 
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/can/netlink.h>
#include <math.h>
#include <net/if.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "mhal_can.h"
#include "queue.h"


#define CAN_STD_ID_MASK (0x7FF)
#define CAN_EXT_ID_MASK (0x1FFFFFFF)
#define CAN_MSG_ID_EXT_FRAME_FORMAT_BIT (1UL << 31)
#define CAN_MSG_ID_CAN_FRAME_RTR_BIT (1UL << 31)
#define CAN_FRAME_SIZE_BYTES   (sizeof(struct can_frame))


typedef enum {
    CAN_STATE_OK,
    CAN_STATE_BUSY,
    CAN_STATE_ERROR,
} CanState;

typedef struct {
    volatile CanState state;
//    mHalTimerObj timer;
    mHalStatus pending_error;
    uint32_t error_count;
    bool loopback;
} CanStateMachine;

typedef struct _can_t {
    int fd; // File descriptor for the CAN interface
    char name[CAN_INTERFACE_NAME_MAX_LENGTH]; // Name of the CAN interface (e.g, "can0")
    uint32_t bitrate; // Bitrate in bits per second
    CanVersion can_version;
    Queue tx_queue; // Queue for outgoing CAN frames
    Queue rx_queue; // Queue for incoming CAN frames
    bool fd_support;
    CanStateMachine state_machine;
} can_t;

mHalStatus mHalCan_Init(Can* pCan, const CanConfig* config) {
    if (!pCan || !config || config->bitrate == 0 || config->queue_size == 0) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    if (config->can_version != CAN_VER_2_0_A && config->can_version != CAN_VER_2_0_B) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    if (config->fd_support && config->can_version != CAN_VER_2_0_B) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Allocate memory for the CAN instance
    Can new_can = calloc(1, sizeof(can_t));
    if (!new_can) {
        return MHAL_STATUS_ERROR;
    }

    // create queues for TX and RX
    if (Queue_Create(&new_can->tx_queue, CAN_FRAME_SIZE_BYTES, config->queue_size) != MHAL_STATUS_OK) {
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    if (Queue_Create(&new_can->rx_queue, CAN_FRAME_SIZE_BYTES, config->queue_size) != MHAL_STATUS_OK) {
        Queue_Destroy(&new_can->tx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    size_t len = strlen(config->name);
    memcpy(new_can->name, config->name, len + 1);

    // create CAN socket and configure for non-blocking I/O
    new_can->fd = socket(PF_CAN, SOCK_RAW | SOCK_NONBLOCK, CAN_RAW);
    if (new_can->fd < 0) {
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    int buffer_size = config->socket_buffer_size;
    if (setsockopt(new_can->fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    if (setsockopt(new_can->fd, SOL_SOCKET, SO_RCVBUF, &buffer_size, sizeof(buffer_size)) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    // get buffer size
    socklen_t optlen = sizeof(buffer_size);
    if (getsockopt(new_can->fd, SOL_SOCKET, SO_SNDBUF, &buffer_size, &optlen) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    printf("<mHalCan> CAN socket buffer size: %d bytes\n", buffer_size);

    if (fcntl(new_can->fd, F_SETFL, O_NONBLOCK) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    struct ifreq ifr;
    memcpy(ifr.ifr_name, new_can->name, strlen(new_can->name) + 1);
    if (ioctl(new_can->fd, SIOCGIFINDEX, &ifr) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    struct sockaddr_can addr;
    memset(&addr, 0, sizeof(addr));
    addr.can_family = AF_CAN;
    addr.can_ifindex = ifr.ifr_ifindex;
    if (bind(new_can->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }

    // set the bitrate
    new_can->bitrate = config->bitrate;
#if defined(RASPBERRY_PI) && defined(HW_MCP2515)
    // For Raspberry Pi with MCP2515 SPI-to-CAN bridge, hardware setup is setup
    // and configured using a dtoverlay (/boot/firmware/config.txt):
    //   dtoverlay=mcp2515-can0,oscillator=16000000,interrupt=12,spimaxfrequency=10000000
    //
    // This overlay configures the SPI driver and maps the Spi port to the CAN
    // interface, can0. The bitrate is set using the ip link command:
    //   sudo ip link set can0 up type can bitrate <bitrate>
    //
    // NOTE: The selected bitrate must be one that can be supported by the
    //       oscilator frequency driving the MCP2515.
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "sudo ip link set %s up type can bitrate %d", new_can->name, new_can->bitrate);
    FILE* fp = popen(buffer, "r");
    if (!fp) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        fprintf(stderr, "<mHalCan> Failed to set bitrate for CAN interface '%s'\n", new_can->name);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    bool bitrate_already_set = false;
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "Device or resource busy") != NULL) {
            bitrate_already_set = true;
            break;
        }
    }
    pclose(fp);
    if (bitrate_already_set) {
        // check that bitrate already set is the desired bitrate
        sprintf(buffer, "ip -details link show %s", new_can->name);
        fp = popen(buffer, "r");
        if (!fp) {
            close(new_can->fd);
            Queue_Destroy(&new_can->tx_queue);
            Queue_Destroy(&new_can->rx_queue);
            free(new_can);
            fprintf(stderr, "<mHalCan> Failed to verify bitrate for CAN interface '%s'\n", new_can->name);
            return MHAL_STATUS_ERROR;
        }
        uint32_t actual_bitrate = 0;
        float sample_point = 0.0f;
        while (fgets(buffer, sizeof(buffer), fp)) {
            if (strstr(buffer, "bitrate") != NULL) {
                sscanf(buffer, " bitrate %u sample-point %f", &actual_bitrate, &sample_point);
                if (actual_bitrate != new_can->bitrate) {
                    pclose(fp);
                    close(new_can->fd);
                    Queue_Destroy(&new_can->tx_queue);
                    Queue_Destroy(&new_can->rx_queue);
                    fprintf(stderr, "<mHalCan> Bitrate mismatch for CAN interface '%s': expected %d, got %u\n",
                        new_can->name, new_can->bitrate, actual_bitrate);
                    free(new_can);
                    return MHAL_STATUS_ERROR;
                }
                break;
            }
        }
        pclose(fp);
    }
    // set txqueuelen
    snprintf(buffer, sizeof(buffer), "sudo ip link set %s txqueuelen %lu", new_can->name, config->socket_txqueuelen);
    fp = popen(buffer, "r");
    if (!fp) {
        close(new_can->fd);
        Queue_Destroy(&new_can->tx_queue);
        Queue_Destroy(&new_can->rx_queue);
        fprintf(stderr, "M<HAL_CAN> Failed to set txqueuelen for CAN interface '%s'\n", new_can->name);
        free(new_can);
        return MHAL_STATUS_ERROR;
    }
    while (fgets(buffer, sizeof(buffer), fp)) {
        if (strstr(buffer, "Device or resource busy") != NULL) {
            // txqueuelen already set
            break;
        }
    }
    pclose(fp);
#else
    // For non-Raspberry Pi platforms, we need to set the bitrate using ioctl
    #error TODO: Implement bitrate setting for non-Raspberry Pi platforms
#endif
    new_can->can_version = config->can_version;

    // initialize the state machine
    new_can->state_machine.state = CAN_STATE_OK;
    new_can->state_machine.pending_error = MHAL_STATUS_OK;
    new_can->state_machine.error_count = 0;
    new_can->state_machine.loopback = false;

    *pCan = new_can;

    printf("<mHalCan> Initialized CAN interface on %s\n", new_can->name);

    return MHAL_STATUS_OK;
}

mHalStatus mHalCan_Deinit(Can* pCan) {
    if (!pCan || !*pCan) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    Can can_instance = *pCan;

    // Close the CAN socket
    if (can_instance->fd >= 0) {
        close(can_instance->fd);
    }
    // Destroy the queues
    mHalStatus status = Queue_Destroy(&can_instance->tx_queue);
    if (status != MHAL_STATUS_OK) {
        return status;
    }
    status = Queue_Destroy(&can_instance->rx_queue);
    if (status != MHAL_STATUS_OK) {
        return status;
    }
    printf("<mHalCan> Deinitialized CAN interface on '%s'\n", can_instance->name);

    // Free the CAN instance
    free(can_instance);
    *pCan = NULL;
    return MHAL_STATUS_OK;
}

size_t mHalCan_GetSizeFromDlc(CanDlc can_dlc) {
    size_t size = 0;

    switch(can_dlc) {
        case CAN_DLC_BYTES_0:
        case CAN_DLC_BYTES_1:
        case CAN_DLC_BYTES_2:
        case CAN_DLC_BYTES_3:
        case CAN_DLC_BYTES_4:
        case CAN_DLC_BYTES_5:
        case CAN_DLC_BYTES_6:
        case CAN_DLC_BYTES_7:
        case CAN_DLC_BYTES_8:
            size = can_dlc;
            break;
        case CAN_DLC_BYTES_12:
            size = 12;
            break;
        case CAN_DLC_BYTES_16:
            size = 16;
            break;
        case CAN_DLC_BYTES_20:
            size = 20;
            break;
        case CAN_DLC_BYTES_24:
            size = 24;
            break;
        case CAN_DLC_BYTES_32:
            size = 32;
            break;
        case CAN_DLC_BYTES_48:
            size = 48;
            break;
        case CAN_DLC_BYTES_64:
            size = 64;
            break;
        default:
            break;
    }

    return size;
}

CanDlc mHalCan_GetDlcFromSize(size_t size) {
    if (size <= CAN_DLC_BYTES_8) {
        return (CanDlc)size; 
    }
    
    switch(size) {
        case 12:  return CAN_DLC_BYTES_12;
        case 16:  return CAN_DLC_BYTES_16;
        case 20:  return CAN_DLC_BYTES_20;
        case 24:  return CAN_DLC_BYTES_24;
        case 32:  return CAN_DLC_BYTES_32;
        case 48:  return CAN_DLC_BYTES_48;
        case 64:  return CAN_DLC_BYTES_64;
        default:  break;
    }
    return CAN_DLC_BYTES_INVALID;
}

/**
 * @brief Write a CAN frame to the CAN interface.
 * 
 * @param can Pointer to the Can instance
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monekyHAL error
 */
static mHalStatus mHalCan_WriteCanFrame(Can can) {
    if (!can) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Check if the CAN interface is enabled
    if (can->fd < 0) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    // get frame from tx queue
    if (!Queue_IsEmpty(can->tx_queue)) {
        struct can_frame frame = {0};
        if (Queue_Dequeue(can->tx_queue, &frame) != MHAL_STATUS_OK) {
            printf("<HAL_CAN> Tx dequeue Failed\n");
            return MHAL_STATUS_ERROR_NO_DATA;
        }
        // Write the CAN frame to the socket
        ssize_t bytes_written = write(can->fd, (void*) &frame, sizeof(struct can_frame));
        if ((bytes_written < 0) || ((size_t)bytes_written != sizeof(struct can_frame))) {
            printf("<mHalCan> Write failed: %s\n", strerror(errno));
            return MHAL_STATUS_ERROR_IO;
        }
    }

    return MHAL_STATUS_OK;
}

/**
 * @brief Read a CAN frame from the CAN interface.
 * 
 * @param can Pointer to the Can instance
 * 
 * @return HMAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
static mHalStatus mHalCan_ReadCanFrame(Can can) {
    if (!can) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Check if the CAN interface is enabled
    if (can->fd < 0) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    // Read a CAN frame from the socket
    struct can_frame frame = {0};
    ssize_t bytes_read = read(can->fd, (void*) &frame, sizeof(struct can_frame));
    if (bytes_read < 0) {
        return MHAL_STATUS_ERROR_IO;
    } else if (bytes_read == 0) {
        return MHAL_STATUS_ERROR_NO_DATA;
    } else if ((size_t)bytes_read != sizeof(struct can_frame)) {
        return MHAL_STATUS_ERROR_IO;
    }

    // Enqueue the received frame to the RX queue
    if (Queue_Enqueue(can->rx_queue, &frame) != MHAL_STATUS_OK) {
        return MHAL_STATUS_ERROR;
    }

    return MHAL_STATUS_OK;
}   

mHalStatus mHalCan_SendCanFrame(Can can, struct can_frame* frame) {
    if (!can || !frame) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Queue the frame for transmission
    if (Queue_Enqueue(can->tx_queue, frame) != MHAL_STATUS_OK) {
        return MHAL_STATUS_ERROR;
    }

    return MHAL_STATUS_OK;
}

mHalStatus mHalCan_GetCanFrame(Can can, struct can_frame* frame) {
    if (!can || !frame) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Dequeue a frame from the RX queue
    if (Queue_IsEmpty(can->rx_queue)) {
        return MHAL_STATUS_ERROR_NO_DATA;
    }   
    if (Queue_Dequeue(can->rx_queue, frame) != MHAL_STATUS_OK) {
        return MHAL_STATUS_ERROR;
    }

    return MHAL_STATUS_OK;
}

/**
 * @brief Check if the CAN interface is ready to transmit data.
 * 
 * @param can Pointer to the Can instance
 * 
 * @return true if the CAN interface is ready to transmit, false otherwise
 */
static bool IsCanEnabled(Can can) {
    return (can && can->fd >= 0);
}

mHalStatus mHalCan_StateMachine(Can can) {
    mHalStatus retval = MHAL_STATUS_ERROR;
    if (!can) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    if (!IsCanEnabled(can)) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    switch (can->state_machine.state) {
        case CAN_STATE_OK:
            // send data if data available
            switch (mHalCan_WriteCanFrame(can)) {
                case MHAL_STATUS_OK:
                case MHAL_STATUS_ERROR_NO_DATA:
                    retval = MHAL_STATUS_OK;
                    break;
                default:
                    // Some other error occurred
                    can->state_machine.pending_error = retval;
                    can->state_machine.state = CAN_STATE_ERROR;
                    retval = MHAL_STATUS_BUSY;
                    break;
            }

            // read data if available
            switch (mHalCan_ReadCanFrame(can)) {
                case MHAL_STATUS_OK:
                case MHAL_STATUS_ERROR_NO_DATA:
                    retval = MHAL_STATUS_OK;
                    break;
                default:
                    // Some other error occurred
                    can->state_machine.pending_error = retval;
                    can->state_machine.state = CAN_STATE_ERROR;
                    retval = MHAL_STATUS_BUSY;
                    break;
            }
            break;
        case CAN_STATE_ERROR:
        default:
            can->state_machine.error_count++;
            can->state_machine.state = CAN_STATE_OK; // Reset state to OK after handling error
            retval = can->state_machine.pending_error;
            can->state_machine.pending_error = MHAL_STATUS_OK; // Reset pending error
            break;
    }

    return retval;
}
