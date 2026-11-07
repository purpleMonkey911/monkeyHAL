/**
 * @file mhal_i2c.h
 * 
 * @brief I2C Abstraction Layer for monkeyHAL STM32 
 *        MCUs supported:
 *        - STM32F3xx (TODO)
 *        - STM32G4xx
 *        - STM32L4xx
 * 
 *        After initialization, I2C operations are handled by the monkeyHAL I2C
 *        state machine.  The state machine behaves as follows:
 *        -  I2C_STATE_OK state until a transaction is pending.
 *        -  I2C_STATE_PENDING state after a call to HalI2c_Xfer() creates a
 *           pending transaction.  The transaction is then handed off to the
 *           MCU I2C peripheral.
 *        -  I2C_STATE_BUSY until the tranaction is completed.  The state
 *           machine waits for an interrupt to indicate the transcation is
 *           completed.  If the transaction exceeds I2C_XFER_TIMEOUT_US, the
 *           transaction is aborted and transition to I2C_STATE_ERROR.
 *        -  I2C_STATE_DONE when the interrupt is generated to indicate the
 *           transaction is complete.  The ISR puts the state machine into
 *           this state.  If a callback was provided by the caller, it will
 *           be called at this time.  We go back to I2C_STATE_OK.
 *        -  I2C_STATE_ERROR is an error occurred at any time during the I2C
 *           transaction.  If a callback was provided by the caller, it will be
 *           called at this time.State machine is reset and we go back to
 *           I2C_STATE_OK
 * 
 * @note On STM32, I2C and SMBus share the same registers but do not share the
 *       same handle on STM32 HAL.  This can create a conflict if a project has
 *       both I2C and SMBus devices on the same I2C bus.  Therefore, the I2C and
 *       SMBus state machines must check if the bus is free.  Furthermore, this
 *       check must be done at the register level.  For I2C, this check is done
 *       with I2c_IsBusFree().  The methodology is that we check the PE bit
 *       (Peripheral Enable) of the CR1 register.  The means that both I2C and
 *       SMBus must disable this bit after every transaction.  For I2C, This is
 *       done with a call to __HAL_I2C_DISABLE().
 * 
 * @note only master mode is supported
 *
 * @warning This has only been tested on the G4
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __MHAL_I2C_H__
#define __MHAL_I2C_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "mhal_gpio.h"
#include "mhal_status.h"
#include "mhal_interrupt.h"

typedef enum {
    I2C_INDEX_1 = 0,
#ifndef STM32L432xx
    I2C_INDEX_2 = 1,
#endif // !STM32L432xx
#ifdef STM32L432xx
    I2C_INDEX_3 = 1,
#endif // STM32L432xx
#if defined(STM32G474xx) || defined(STM32L496xx)
    I2C_INDEX_3 = 2,
    I2C_INDEX_4 = 3,
#endif // STM32G474xx || STM32L496xx
    NUM_I2C,
} I2cIndex;

typedef enum {
    I2C_MODE_STANDARD,  // 100kbps
    I2C_MODE_FAST,      // 400kbps
    I2C_MODE_FAST_PLUS, // 1Mbps
} I2cMode;

typedef enum {
    I2C_ADDRESSING_MODE_7BITS,
    I2C_ADDRESSING_MODE_10BITS,
} I2cAddressingMode;

// I2cXferMode is dictated by the type of device that is connected.
// - I2C_XFER_MODE_STREAM is used for a device where only data is streamed (in
//   either direction) and no memory address or register is specified.
//
// - I2C_XFER_MODE_MEMORY is used for a device where data is transferred (in
//   either direction) starting from a specified memory address or register.
//   This is typically the mode we would use.  NOTE: for a read transaction,
//   the transfer starts as a write, a restart occurs, then the transfer
//   becomes a read.  In the STM32 HAL implementation, the write part of this
//   read transaction is blocking then non-blocking with the read part.
//
// - I2C_XFER_MODE_SEQ is used when transfer requires a sequence of
//   transmissions where a restart(s) is needed in the transaction.  This may
//   be useful if we need a completely non-blocking read transaction.  We may
//   explore this if the I2C_XFER_MODE_MEMORY is deemed problematic.  For now,
//   we will not support this mode.
typedef enum {
    I2C_XFER_MODE_STREAM,
    I2C_XFER_MODE_MEMORY,
//  I2C_XFER_MODE_SEQ,
} I2cXferMode;

typedef void (*mHalI2c_XferDone_cb)(void* ctx);
typedef void (*mHalI2c_XferError_cb)(void* ctx);

typedef struct {
    struct {
        Gpio scl;
        GpioConfig scl_config;
        Gpio sda;
        GpioConfig sda_config;
    } gpio;
    I2cIndex index;
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
 * @brief Set interrupt priority.
 *
 * @param i2c    I2c instance
 * @param prior  interrupt priority level
 *
 * @return MHAL_STATUS_OK if successfully initialized, otherwise error state
 */
extern mHalStatus mHalI2c_SetInterruptPriority(I2c i2c, mHalInterruptPriority prior);

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
 * @return MHAL_STATUS_OK if successfully executed, otherwise monkeyHAL error
 */
extern mHalStatus mHalI2c_StateMachine(I2c i2c);

#endif // __MHAL_I2C_H__
