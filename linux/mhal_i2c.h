/**
 * @file mhal_i2c.h
 * 
 * @brief I2C Abstraction Layer for monkeyHAL
 * 
 *        This is a user-space linux device driver.  The design of this driver
 *        is meant to work around the lack of kernel space access to create a
 *        non-blocking device driver.  Use of this driver is a follows:
 *        - declare a I2c instance
 *        - initialize the instance (mHalI2c_Init())
 *        - exercise the Uart state machine (mHalI2c_StateMachine()) as
 *          quickly as poosible (e.g. main while loop)
 *        - call mHalI2c_Xfer() to transfer data
 * 
 *        The application provide a persistent buffer (when calling
 *        mHalI2c_Xfer()) that will contain any data to send or receive any
 *        data to receive.  The I2cXfer type (also passed when calling
 *        mHalI2c_Xfer()) contains all I2C settings for the transfer.  It is
 *        sent with each transfer because there may be multiple devices on the
 *        bus with different I2C settings.
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
 * @note only master mode is supported
 *
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __MHAL_I2C_H__
#define __MHAL_I2C_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "mhal_status.h"

// NOTE: the i2c bus clock cannot be set in user space.  It is set by the
//       device tree.  So I2cMode is just here for completeness (and possible
//       update for kernel space modification).
typedef enum {
    I2C_MODE_STANDARD,  // 100kbps
    I2C_MODE_FAST,      // 400kbps
    I2C_MODE_FAST_PLUS, // 1Mbps
} I2cMode;

typedef enum {
    I2C_ADDRESSING_MODE_7BITS,
    I2C_ADDRESSING_MODE_10BITS,
} I2cAddressingMode;


typedef enum {
    I2C_XFER_MODE_STREAM,  // i2c transactions only send/receive data
    I2C_XFER_MODE_MEMORY,  // i2c transactions send a register address
                           // before data
} I2cXferMode;

typedef void (*mHalI2c_XferDone_cb)(void* ctx);
typedef void (*mHalI2c_XferError_cb)(void* ctx);

typedef struct {
    const char* i2c_bus;
} I2cConfig;

// I2cXfer is used to define the details of an I2C transaction.  This will be
// defined for each transaction since the I2C bus may have multiple slaves (each
// with its own I2C configuration).
typedef struct {
    I2cMode mode;
    I2cAddressingMode address_mode;
    uint16_t address;    // slave address
    bool master_mode;    // true if MCU is master, false if MCU is slave.  For
                         // now, we only support Master mode.
    I2cXferMode xfer_mode;
    bool write;          // true if this transfer is a write transaction, false
                         // if transfer is a read transaction.

    // these two members are only used for I2C_XFER_MODE_MEMORY
    uint16_t reg;        // slave dev register or memory address to access
    uint16_t reg_size;   //  size of the reg address.  Either 1 or 2 (bytes)

    mHalI2c_XferDone_cb done_cb;    // callback for when the transfer is completed.
                                   // Can be NULL.
    void* done_cb_ctx;             // context for done_cb.  Can be NULL
    mHalI2c_XferError_cb error_cb;  // callback if an error occurs when transfer
                                   // does not succeed.  Can be NULL.
    void* error_cb_ctx;            // context for error_cb.  Can be NULL

} I2cXfer;

typedef struct _I2c_t I2c_t;
typedef I2c_t* I2c;

/**
 * @brief Initialize I2c module.
 * 
 * @param [in] pI2c   pointer to I2c instance
 * @param [in] config pointer to I2cConfig
 * 
 * @return MHAL_STATUS_OK if successfully initialized, otherwise monkeyHAL error
 */
extern mHalStatus mHalI2c_Init(I2c* pI2c, I2cConfig* config);

/**
 * @brief Deinitialize I2c module.
 * 
 * @param [in] pI2c   pointer to I2c instance
 * 
 * @return MHAL_STATUS_OK if successfully deinitialized, otherwise monkeyHAL error
 */
extern mHalStatus mHalI2c_Deinit(I2c* pI2c);

/**
 * @brief Initiate an I2C transfer.
 *        This function will set up the I2C transaction and hand off to the I2c
 *        state machine.
 * 
 * @param [in] i2c    I2c instance
 * @param [in] xfer   pointer to I2cXfer data
 * @param [in] buffer data to be transferred
 * @param [in] size   size of data to be transfered (bytes)
 * 
 * @note The buffer that is passed to this module must remain persistent until the
 *       transaction is complete! 
 * 
 * @return MHAL_STATUS_OK if successfully initiated, otherwise monkeyHAL error
 */
extern mHalStatus mHalI2c_Xfer(I2c i2c, I2cXfer* xfer, uint8_t* buffer, size_t size);

/**
 * @brief I2C state machine.
 * 
 * @param [in] i2c  I2c instance
 * 
 * @return mHalStatus depending on execution
 */
extern mHalStatus mHalI2c_StateMachine(I2c i2c);

/**
 * @brief Get a file descriptor that becomes readable (POLLIN) when a transfer
 *        completes, for use with poll()/select()/epoll.
 *
 *        When it is readable, call mHalI2c_StateMachine() to collect the
 *        result; that also clears the event.  Do not read() the fd yourself.
 *
 * @param i2c I2c instance
 *
 * @return file descriptor, or -1 if i2c is NULL
 */
extern int mHalI2c_GetEventFd(I2c i2c);

#endif // __MHAL_I2C_H__
