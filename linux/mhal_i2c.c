/**
 * @file mhal_i2c.c
 * 
 * @brief I2C Abstraction Layer for monkeyHAL
 *
 * @note only master mode is supported
 *
 *        Linux i2c-dev has no non-blocking interface: read(), write() and
 *        ioctl(I2C_RDWR) block until the bus transaction completes.  To keep
 *        mHalI2c_Xfer() non-blocking (like the UART and CAN drivers), each I2c
 *        instance owns a worker thread that performs the blocking call.
 *
 *        Thread model:
 *        - mHalI2c_Xfer() (caller's thread) queues one transfer: OK -> PENDING
 *        - worker thread picks it up:                PENDING -> XFER
 *          and runs the blocking transfer:           XFER -> DONE / ERROR
 *        - mHalI2c_StateMachine() (caller's thread)   DONE / ERROR -> OK
 *          and invokes done_cb / error_cb.
 *        Callbacks therefore always run in the thread that calls
 *        mHalI2c_StateMachine(), never in the worker thread.
 *
 *        When the worker finishes it also signals an eventfd
 *        (mHalI2c_GetEventFd()), so the caller can sleep in poll() instead
 *        of spinning on mHalI2c_StateMachine().
 *
 *        The caller must not touch the transfer buffer until the transfer
 *        completes (done_cb / error_cb, or mHalI2c_StateMachine() returns
 *        DONE / an error) -- same contract as a DMA transfer.
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <fcntl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "mhal_i2c.h"

typedef enum {
    I2C_STATE_OK,       // idle, ready for a new transfer
    I2C_STATE_PENDING,  // transfer queued, waiting for the worker thread
    I2C_STATE_XFER,     // worker thread is on the bus
    I2C_STATE_DONE,     // transfer succeeded, waiting for mHalI2c_StateMachine()
    I2C_STATE_ERROR,    // transfer failed, waiting for mHalI2c_StateMachine()
} I2cState;

// state and pending_error are shared with the worker thread and are only
// accessed with I2c_t.lock held.  The xfer fields are written by
// mHalI2c_Xfer() (state OK) and read by the worker (state XFER); the state
// hand-off under the lock keeps those from overlapping.
typedef struct {
    I2cState state;
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
    int event_fd;            // readable when a transfer is DONE / ERROR
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_cond_t cond;     // signalled when a transfer is queued or on stop
    bool stop;               // tells the worker thread to exit
    I2cStateMachine state_machine;
} I2c_t;

static void* I2c_Worker(void* arg);

/**
 * @brief Clear the transfer-complete event so the event fd is no longer
 *        readable.
 *
 * @param i2c I2c instance
 *
 * @note event_fd is a counter; a single read() resets it to zero.  If the
 *       event is already clear, read() fails with EAGAIN, which is harmless.
 */
static void I2c_ClearEvent(I2c i2c) {
    uint64_t value;
    ssize_t n = read(i2c->event_fd, &value, sizeof(value));
    (void)n;  // EAGAIN if already clear: fine
}

/**
 * @brief Clear the current transfer and return the state machine to idle
 *        (I2C_STATE_OK).
 *
 * @param i2c I2c instance
 *
 * @note The caller must hold i2c->lock (except during mHalI2c_Init(), before
 *       the worker thread exists).
 */
static void I2c_ResetXfer(I2c i2c) {
    i2c->state_machine.state = I2C_STATE_OK;
    i2c->state_machine.pending_error = MHAL_STATUS_OK;
    i2c->state_machine.xfer.buffer = NULL;
    i2c->state_machine.xfer.buffer_size = 0;
    i2c->state_machine.xfer.reg_size = 0;
    i2c->state_machine.xfer.done_cb = NULL;
    i2c->state_machine.xfer.done_cb_ctx = NULL;
    i2c->state_machine.xfer.error_cb = NULL;
    i2c->state_machine.xfer.error_cb_ctx = NULL;
}

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
    i2c->event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (i2c->event_fd == -1) {
        close(i2c->fd);
        free(i2c);
        return MHAL_STATUS_ERROR;
    }

    // initialize state machine
    I2c_ResetXfer(i2c);
    i2c->state_machine.error_count = 0;
    i2c->stop = false;

    // start worker thread
    if (pthread_mutex_init(&i2c->lock, NULL) != 0) {
        close(i2c->event_fd);
        close(i2c->fd);
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    if (pthread_cond_init(&i2c->cond, NULL) != 0) {
        pthread_mutex_destroy(&i2c->lock);
        close(i2c->event_fd);
        close(i2c->fd);
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    if (pthread_create(&i2c->worker, NULL, I2c_Worker, i2c) != 0) {
        pthread_cond_destroy(&i2c->cond);
        pthread_mutex_destroy(&i2c->lock);
        close(i2c->event_fd);
        close(i2c->fd);
        free(i2c);
        return MHAL_STATUS_ERROR;
    }

    *pI2c = i2c;

    printf("<mHalI2c> Initialized i2c on port %s \n", config->i2c_bus);
    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_Deinit(I2c* pI2c) {
    if (pI2c && *pI2c) {
        I2c i2c = *pI2c;

        // stop the worker.  A queued (PENDING) transfer is dropped; a transfer
        // already on the bus (XFER) can't be interrupted, so pthread_join()
        // waits for it to finish.
        pthread_mutex_lock(&i2c->lock);
        i2c->stop = true;
        pthread_cond_signal(&i2c->cond);
        pthread_mutex_unlock(&i2c->lock);
        pthread_join(i2c->worker, NULL);

        pthread_cond_destroy(&i2c->cond);
        pthread_mutex_destroy(&i2c->lock);
        close(i2c->event_fd);
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

    // struct i2c_msg length is 16 bits
    if (xfer->xfer_mode == I2C_XFER_MODE_MEMORY) {
        if ((xfer->reg_size != 1) && (xfer->reg_size != 2)) return MHAL_STATUS_ERROR_INVALID_PARAM;
        if ((size_t)xfer->reg_size + size > UINT16_MAX) return MHAL_STATUS_ERROR_INVALID_PARAM;
    } else if (size > UINT16_MAX) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    pthread_mutex_lock(&i2c->lock);
    if (i2c->state_machine.state != I2C_STATE_OK) {
        pthread_mutex_unlock(&i2c->lock);
        return MHAL_STATUS_BUSY;
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

    // hand the transfer to the worker thread
    i2c->state_machine.state = I2C_STATE_PENDING;
    pthread_cond_signal(&i2c->cond);
    pthread_mutex_unlock(&i2c->lock);
    return MHAL_STATUS_OK;
}

/**
 * @brief Perform a stream transfer: a plain read() or write() to the slave,
 *        with no register address.
 *
 * @param i2c I2c instance
 *
 * @note Blocks until the bus transaction completes.  Only called from the
 *       worker thread.
 *
 * @return MHAL_STATUS_OK if the full buffer was transferred, otherwise
 *         MHAL_STATUS_ERROR_IO
 */
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

/**
 * @brief Perform a memory transfer: a register address followed by data,
 *        done atomically with ioctl(I2C_RDWR).
 *
 *        - read:  [S] addr+W reg [Sr] addr+R data... [P]
 *                 (2 messages, repeated start)
 *        - write: [S] addr+W reg data... [P]
 *                 (1 message, register address prepended to the data)
 *
 * @param i2c I2c instance
 *
 * @note I2C_RDWR addresses each message directly, so I2C_SLAVE/I2C_TENBIT are
 *       not needed (and the slave address need not be free of a kernel
 *       driver).
 * @note Blocks until the bus transaction completes.  Only called from the
 *       worker thread.
 *
 * @return MHAL_STATUS_OK if successful, MHAL_STATUS_ERROR if the write buffer
 *         could not be allocated, otherwise MHAL_STATUS_ERROR_IO
 */
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

/**
 * @brief Worker thread for an I2c instance.  Sleeps until a transfer is
 *        queued (PENDING), runs the blocking transfer (XFER) without holding
 *        the lock, then publishes the result (DONE / ERROR) and signals the
 *        event fd.
 *
 * @param arg I2c instance
 *
 * @note Exits when i2c->stop is set by mHalI2c_Deinit().
 *
 * @return always NULL
 */
static void* I2c_Worker(void* arg) {
    I2c i2c = (I2c)arg;

    pthread_mutex_lock(&i2c->lock);
    for (;;) {
        while (!i2c->stop && (i2c->state_machine.state != I2C_STATE_PENDING)) {
            pthread_cond_wait(&i2c->cond, &i2c->lock);
        }
        if (i2c->stop) {
            break;
        }
        i2c->state_machine.state = I2C_STATE_XFER;
        pthread_mutex_unlock(&i2c->lock);

        // blocking bus transaction; the lock is released so
        // mHalI2c_StateMachine() never waits on the bus
        mHalStatus status;
        if (i2c->state_machine.xfer.xfer_mode == I2C_XFER_MODE_MEMORY) {
            status = I2c_MemoryXfer(i2c);
        } else {
            status = I2c_StreamXfer(i2c);
        }

        pthread_mutex_lock(&i2c->lock);
        if (status != MHAL_STATUS_OK) {
            i2c->state_machine.pending_error = status;
            i2c->state_machine.state = I2C_STATE_ERROR;
        } else {
            i2c->state_machine.state = I2C_STATE_DONE;
        }

        // wake the caller's poll()
        uint64_t one = 1;
        ssize_t n = write(i2c->event_fd, &one, sizeof(one));
        (void)n;  // can only fail if the counter overflows: not possible here
    }
    pthread_mutex_unlock(&i2c->lock);
    return NULL;
}

int mHalI2c_GetEventFd(I2c i2c) {
    return i2c ? i2c->event_fd : -1;
}

mHalStatus mHalI2c_StateMachine(I2c i2c) {
    if (!i2c) return MHAL_STATUS_ERROR_INSTANCE;

    mHalStatus retval = MHAL_STATUS_ERROR;
    mHalI2c_XferDone_cb cb = NULL;  // done_cb or error_cb (same signature)
    void* cb_ctx = NULL;

    pthread_mutex_lock(&i2c->lock);
    switch (i2c->state_machine.state) {
        case I2C_STATE_OK:
            retval = MHAL_STATUS_OK;
            break;
        case I2C_STATE_PENDING:
        case I2C_STATE_XFER:
            // worker thread owns the transfer
            retval = MHAL_STATUS_BUSY;
            break;
        case I2C_STATE_DONE:
            cb = i2c->state_machine.xfer.done_cb;
            cb_ctx = i2c->state_machine.xfer.done_cb_ctx;
            retval = MHAL_STATUS_DONE;
            I2c_ResetXfer(i2c);
            I2c_ClearEvent(i2c);
            break;
        case I2C_STATE_ERROR:
            cb = i2c->state_machine.xfer.error_cb;
            cb_ctx = i2c->state_machine.xfer.error_cb_ctx;
            retval = i2c->state_machine.pending_error;
            i2c->state_machine.error_count++;
            I2c_ResetXfer(i2c);
            I2c_ClearEvent(i2c);
            break;
        default:
            // should not get here
            i2c->state_machine.error_count++;
            I2c_ResetXfer(i2c);
            break;
    }
    pthread_mutex_unlock(&i2c->lock);

    // Callback runs after the reset and outside the lock, so it may start the
    // next transfer with mHalI2c_Xfer().
    if (cb) {
        cb(cb_ctx);
    }

    return retval;
}
