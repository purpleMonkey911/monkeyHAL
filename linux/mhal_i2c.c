/**
 * @file mhal_i2c.c
 * 
 * @brief I2C Abstraction Layer for monkeyHAL
 *
 * @note only master mode is supported
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "mhal_i2c.h"

typedef enum {
    I2C_STATE_OK,
    I2C_STATE_PENDING,
    I2C_STATE_XFER,
    I2C_STATE_DONE,
    I2C_STATE_ERROR,
} I2cState;

typedef struct {
    volatile I2cState state;
    mHalStatus pending_error;
    uint32_t error_count;
    struct {
        I2cMode mode;
        uint16_t address;
        I2cAddressingMode address_mode;
        I2cXferMode xfer_mode;
        bool write;
        uint16_t reg;
        uint16_t reg_size;
        uint8_t* buffer;
        size_t buffer_size;
        mHalI2c_XferDone_cb done_cb;
        void* done_cb_ctx;
        mHalI2c_XferError_cb error_cb;
        void* error_cb_ctx;
    } xfer;
} I2cStateMachine;

typedef struct _I2c_t {
    int fd;
    I2cStateMachine state_machine;
} I2c_t;

mHalStatus mHalI2c_Init(I2c* pI2c, I2cConfig* config) {
    if (!config || !config->i2c_bus) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    if (!pI2c) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    I2c i2c = calloc(1, sizeof(I2c_t));
    if (!i2c) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    // set up instance
    i2c->fd = open(config->i2c_bus, O_RDWR);
    if (i2c->fd == -1) {
        free(i2c);
        return MHAL_STATUS_ERROR_IO;
    }

    // initialize state machine
    i2c->state_machine.state = I2C_STATE_OK;
    i2c->state_machine.pending_error = MHAL_STATUS_OK;
    i2c->state_machine.error_count = 0;

    i2c->state_machine.xfer.buffer = NULL;
    i2c->state_machine.xfer.buffer_size = 0;
    i2c->state_machine.xfer.done_cb = NULL;
    i2c->state_machine.xfer.done_cb_ctx = NULL;
    i2c->state_machine.xfer.error_cb = NULL;
    i2c->state_machine.xfer.error_cb_ctx = NULL;

    *pI2c = i2c;

    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_Deinit(I2c* pI2c) {
    if (pI2c && *pI2c) {
        I2c i2c = *pI2c;
        close(i2c->fd);
        free(i2c);
        *pI2c = NULL;
        return MHAL_STATUS_OK;
    }
    return MHAL_STATUS_ERROR_INSTANCE;
}

mHalStatus mHalI2c_Xfer(I2c i2c, I2cXfer* xfer, uint8_t* buffer, size_t size) {
    if (!xfer || !buffer || !size) return MHAL_STATUS_ERROR_INVALID_PARAM;

    if (!i2c) return MHAL_STATUS_ERROR_INSTANCE;
    if (i2c->state_machine.state != I2C_STATE_OK) return MHAL_STATUS_BUSY;

    // struct i2c_msg length is 16 bits
    if (xfer->xfer_mode == I2C_XFER_MODE_MEMORY) {
        if ((xfer->reg_size != 1) && (xfer->reg_size != 2)) return MHAL_STATUS_ERROR_INVALID_PARAM;
        if ((size_t)xfer->reg_size + size > UINT16_MAX) return MHAL_STATUS_ERROR_INVALID_PARAM;
    } else if (size > UINT16_MAX) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    i2c->state_machine.xfer.mode         = xfer->mode;
    i2c->state_machine.xfer.address      = xfer->address;
    i2c->state_machine.xfer.address_mode = xfer->address_mode;
    i2c->state_machine.xfer.xfer_mode    = xfer->xfer_mode;
    i2c->state_machine.xfer.write        = xfer->write;
    i2c->state_machine.xfer.reg          = xfer->reg;
    i2c->state_machine.xfer.reg_size     = xfer->reg_size;
    i2c->state_machine.xfer.buffer       = buffer;
    i2c->state_machine.xfer.buffer_size  = size;
    i2c->state_machine.xfer.done_cb      = xfer->done_cb;
    i2c->state_machine.xfer.done_cb_ctx  = xfer->done_cb_ctx;
    i2c->state_machine.xfer.error_cb     = xfer->error_cb;
    i2c->state_machine.xfer.error_cb_ctx = xfer->error_cb_ctx;

    i2c->state_machine.state = I2C_STATE_PENDING;
    return MHAL_STATUS_OK;
}

// Stream transfer: plain read() or write() to the slave, no register address.
static mHalStatus I2c_StreamXfer(I2c i2c) {
    bool tenbit = (i2c->state_machine.xfer.address_mode == I2C_ADDRESSING_MODE_10BITS);
    if (ioctl(i2c->fd, I2C_TENBIT, tenbit ? 1 : 0) != 0) {
        return MHAL_STATUS_ERROR_IO;
    }
    if (ioctl(i2c->fd, I2C_SLAVE, i2c->state_machine.xfer.address) != 0) {
        return MHAL_STATUS_ERROR_IO;
    }

    ssize_t size;
    if (i2c->state_machine.xfer.write) {
        size = write(i2c->fd, i2c->state_machine.xfer.buffer, i2c->state_machine.xfer.buffer_size);
    } else {
        size = read(i2c->fd, i2c->state_machine.xfer.buffer, i2c->state_machine.xfer.buffer_size);
    }
    if ((size < 0) || ((size_t)size != i2c->state_machine.xfer.buffer_size)) {
        return MHAL_STATUS_ERROR_IO;
    }
    return MHAL_STATUS_OK;
}

// Memory transfer: register address followed by data, done atomically with
// I2C_RDWR.
// - read:  [S] addr+W reg [Sr] addr+R data... [P]  (2 messages, repeated start)
// - write: [S] addr+W reg data... [P]              (1 message, reg prepended)
// I2C_RDWR addresses each message directly, so I2C_SLAVE/I2C_TENBIT are not
// needed (and the slave address need not be free of a kernel driver).
static mHalStatus I2c_MemoryXfer(I2c i2c) {
    uint16_t reg_size = i2c->state_machine.xfer.reg_size;
    size_t data_size = i2c->state_machine.xfer.buffer_size;
    uint16_t flags = (i2c->state_machine.xfer.address_mode == I2C_ADDRESSING_MODE_10BITS) ? I2C_M_TEN : 0;

    // register address, MSB first
    uint8_t reg_bytes[2];
    if (reg_size == 2) {
        reg_bytes[0] = (uint8_t)(i2c->state_machine.xfer.reg >> 8);
        reg_bytes[1] = (uint8_t)(i2c->state_machine.xfer.reg & 0xFF);
    } else {
        reg_bytes[0] = (uint8_t)(i2c->state_machine.xfer.reg & 0xFF);
    }

    struct i2c_msg msgs[2];
    struct i2c_rdwr_ioctl_data rdwr = { .msgs = msgs, .nmsgs = 0 };
    uint8_t* tx = NULL;

    if (i2c->state_machine.xfer.write) {
        tx = malloc(reg_size + data_size);
        if (!tx) {
            return MHAL_STATUS_ERROR;
        }
        memcpy(tx, reg_bytes, reg_size);
        memcpy(tx + reg_size, i2c->state_machine.xfer.buffer, data_size);

        msgs[0].addr  = i2c->state_machine.xfer.address;
        msgs[0].flags = flags;
        msgs[0].len   = (uint16_t)(reg_size + data_size);
        msgs[0].buf   = tx;
        rdwr.nmsgs = 1;
    } else {
        msgs[0].addr  = i2c->state_machine.xfer.address;
        msgs[0].flags = flags;
        msgs[0].len   = reg_size;
        msgs[0].buf   = reg_bytes;

        msgs[1].addr  = i2c->state_machine.xfer.address;
        msgs[1].flags = flags | I2C_M_RD;
        msgs[1].len   = (uint16_t)data_size;
        msgs[1].buf   = i2c->state_machine.xfer.buffer;
        rdwr.nmsgs = 2;
    }

    // I2C_RDWR returns the number of messages transferred
    int ret = ioctl(i2c->fd, I2C_RDWR, &rdwr);
    free(tx);
    if (ret != (int)rdwr.nmsgs) {
        return MHAL_STATUS_ERROR_IO;
    }
    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_StateMachine(I2c i2c) {
    mHalStatus retval = MHAL_STATUS_ERROR;
    if (!i2c) return MHAL_STATUS_ERROR_INSTANCE;

    mHalStatus status;
    switch (i2c->state_machine.state) {
        case I2C_STATE_OK:
            retval = MHAL_STATUS_OK;
            break;
        case I2C_STATE_PENDING:
            if (i2c->state_machine.xfer.xfer_mode == I2C_XFER_MODE_MEMORY) {
                status = I2c_MemoryXfer(i2c);
            } else {
                status = I2c_StreamXfer(i2c);
            }
            if (status != MHAL_STATUS_OK) {
                i2c->state_machine.pending_error = status;
                i2c->state_machine.state = I2C_STATE_ERROR;
            } else {
                i2c->state_machine.state = I2C_STATE_DONE;
            }
            retval = MHAL_STATUS_BUSY;
            break;
        case I2C_STATE_XFER:
            // RA TODO
            retval = MHAL_STATUS_BUSY;
            break;
        case I2C_STATE_DONE:
            if (i2c->state_machine.xfer.done_cb) {
                i2c->state_machine.xfer.done_cb(i2c->state_machine.xfer.done_cb_ctx);
            }
            retval = MHAL_STATUS_DONE;
            i2c->state_machine.state = I2C_STATE_OK;
            i2c->state_machine.xfer.buffer = NULL;
            i2c->state_machine.xfer.buffer_size = 0;
            i2c->state_machine.xfer.reg_size = 0;
            i2c->state_machine.xfer.done_cb = NULL;
            i2c->state_machine.xfer.done_cb_ctx = NULL;
            i2c->state_machine.xfer.error_cb = NULL;
            i2c->state_machine.xfer.error_cb_ctx = NULL;
            break;
        case I2C_STATE_ERROR:
            if (i2c->state_machine.xfer.error_cb) {
                i2c->state_machine.xfer.error_cb(i2c->state_machine.xfer.error_cb_ctx);
            }
            retval = i2c->state_machine.pending_error;
            // fall-thru
        default:
            i2c->state_machine.error_count++;
            i2c->state_machine.pending_error = MHAL_STATUS_OK;
            i2c->state_machine.state = I2C_STATE_OK;
            i2c->state_machine.xfer.buffer = NULL;
            i2c->state_machine.xfer.buffer_size = 0;
            i2c->state_machine.xfer.reg_size = 0;
            i2c->state_machine.xfer.done_cb = NULL;
            i2c->state_machine.xfer.done_cb_ctx = NULL;
            i2c->state_machine.xfer.error_cb = NULL;
            i2c->state_machine.xfer.error_cb_ctx = NULL;
            break;
    }

    return retval;
}
