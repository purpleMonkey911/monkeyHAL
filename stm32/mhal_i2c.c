/**
 * @file mhal_i2c.c
 * 
 * @brief I2C Abstraction Layer for monkeyHAL STM32 
 *        MCUs supported:
 *        - STM32F3xx (TODO)
 *        - STM32G4xx
 *        - STM32L4xx
 * 
 *        After initialization, I2C operations are handled by the mttr-hal I2C
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

#if defined(STM32CubeF3)
  #include "stm32f3xx_hal.h"
  #include "stm32f3xx_hal_i2c.h"
#elif defined(STM32CubeG4)
  #include "stm32g4xx_hal.h"
  #include "stm32g4xx_hal_i2c.h"
#elif defined (STM32CubeL4)
  #include "stm32l4xx_hal.h"
  #include "stm32l4xx_hal_i2c.h"
#else
  #error "MCU not supported!"
#endif

#include <stdlib.h>

#include "_mhal_internal.h"
#include "mhal_i2c.h"
#include "mhal_timer.h"

// Values for the STM32 I2C Timing Register
// These values were calculated by STM32CubeMX
// See STM32 reference manual for further details
#if (CPU_CLOCK_HZ == 16000000)
  #define I2C_TIMING_REG_STD       0x00303D5B
  #define I2C_TIMING_REG_FAST      0x0010061A
  #define I2C_TIMING_REG_FAST_PLUS 0x00000107
#elif (CPU_CLOCK_HZ == 64000000)
  #define I2C_TIMING_REG_STD       0x10707DBC
  #define I2C_TIMING_REG_FAST      0x00602173
  #define I2C_TIMING_REG_FAST_PLUS 0x00300B29
#elif (CPU_CLOCK_HZ == 168000000)
  #define I2C_TIMING_REG_STD       0x60505F8C
  #define I2C_TIMING_REG_FAST      0x20501E65
  #define I2C_TIMING_REG_FAST_PLUS 0x00802F62
#else
  #error "Selected CPU clock speed not supported!"
#endif // CPU_CLOCK_HZ

// flags for enabling/disabling Fast Mode Plus
static const uint32_t fmp_flag[NUM_I2C] = {
    I2C_FASTMODEPLUS_I2C1,
#ifndef STM32L432xx
    I2C_FASTMODEPLUS_I2C2,
#endif // !STM32L432xx
#ifdef STM32L432xx
    I2C_FASTMODEPLUS_I2C3,
#endif // STM32L432xx
#if defined (STM32G474xx) || defined(STM32L496xx)
    I2C_FASTMODEPLUS_I2C3,
    I2C_FASTMODEPLUS_I2C4,
#endif
};

#define I2C_XFER_TIMEOUT_US 200000  // 200 msecs

typedef enum {
    I2C_STATE_OK,
    I2C_STATE_PENDING,
    I2C_STATE_XFER,
    I2C_STATE_DONE,
    I2C_STATE_ERROR,
} I2cState;

typedef struct {
    volatile I2cState state;
    mHalTimerObj timer;
    mHalStatus pending_error;
    uint32_t error_count;
    struct {
        I2cMode mode;
        uint16_t address;
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
    I2cIndex index;
    I2C_HandleTypeDef hi2c;
    I2cStateMachine state_machine;
} I2c_t;
static I2c i2c_handle_table[NUM_I2C] = {0};


/**
 * @brief Get hardware-specific I2C peripheral.
 * 
 * @param [in] index  index of which I2C to get
 * 
 * @return pointer to I2C peripheral
 */
static I2C_TypeDef* GetI2cBase(I2cIndex index) {
    I2C_TypeDef* retval = NULL;
    switch (index) {
        case I2C_INDEX_1:
            retval = I2C1;
            break;
#ifndef STM32L432xx
        case I2C_INDEX_2:
            retval = I2C2;
            break;
#endif // !STM32L432xx
#ifdef STM32L432xx
        case I2C_INDEX_3:
            retval = I2C3;
            break;
#endif // STM32L432xx
#if defined(STM32G474xx) || defined(STM32L496xx)
        case I2C_INDEX_3:
            retval = I2C3;
            break;
        case I2C_INDEX_4:
            retval = I2C4;
            break;
#endif
        default:
            break;
    }
    return retval;
}

/**
 * @brief Get I2C interrupt request vector.
 * 
 * @param [in] index  index of which I2C to get
 * 
 * @return I2C interrupt request vector
 */
static IRQn_Type GetI2cIRQn(I2cIndex index) {
    IRQn_Type retval = 0;

    switch (index) {
        case I2C_INDEX_1:
            retval = I2C1_EV_IRQn;
            break;
#ifndef STM32L432xx
        case I2C_INDEX_2:
            retval = I2C2_EV_IRQn;
            break;
#endif // !STM32L432xx
#ifdef STM32L432xx
        case I2C_INDEX_3:
            retval = I2C3_EV_IRQn;
            break;
#endif // STM32L432xx
#if defined(STM32G474xx) || defined(STM32L496xx)
        case I2C_INDEX_3:
            retval = I2C3_EV_IRQn;
            break;
        case I2C_INDEX_4:
            retval = I2C4_EV_IRQn;
            break;
#endif // STM32G474xx || STM32L496xx
        default:
            break;
    }

    return retval;
}

/**
 * @brief Get I2C error interrupt request vector.
 * 
 * @param [in] index  index of which I2C to get
 * 
 * @return I2C error interrupt request vector
 */
static IRQn_Type GetI2cErrorIRQn(I2cIndex index) {
    IRQn_Type retval = 0;

    switch (index) {
        case I2C_INDEX_1:
            retval = I2C1_ER_IRQn;
            break;
#ifndef STM32L432xx
        case I2C_INDEX_2:
            retval = I2C2_ER_IRQn;
            break;
#endif // !STM32L432xx
#ifdef STM32L432xx
        case I2C_INDEX_3:
            retval = I2C3_ER_IRQn;
            break;
#endif // STM32L432xx
#if defined(STM32G474xx) || defined(STM32L496xx)
        case I2C_INDEX_3:
            retval = I2C3_ER_IRQn;
            break;
        case I2C_INDEX_4:
            retval = I2C4_ER_IRQn;
            break;
#endif // STM32G474xx || STM32L496xx
        default:
            break;
    }

    return retval;
}

/**
 * @brief Get I2c instance from I2C handle.
 * 
 * @param [in] hi2c handle to I2C instance
 * 
 * @return I2c instance associated with the I2C handle
 */
static I2c GetI2cFromHandle(I2C_HandleTypeDef *hi2c) {
    I2c retval = NULL;

    for (int i = I2C_INDEX_1; i < NUM_I2C; i++) {
        if (i2c_handle_table[i] && &i2c_handle_table[i]->hi2c == hi2c) {
            retval = i2c_handle_table[i];
            break;
        }
    }
    return retval;
}

/**
 * @brief Check if I2C bus is available for a transfer.
 * 
 * @note  This check is needed as it is not sufficient to check only the state
 *        of the state machine nor the I2C handle state because SMBus shares
 *        the same I2C peripheral.  So the check must be done at the register
 *        level.
 * 
 * @param [in] pI2c  pointer I2c instance
 * 
 * @return true if bus is available, otherwise false
 */
static bool I2c_IsBusFree(I2c i2c) {
    if (i2c) {
        return ((i2c->hi2c.Instance->CR1 & I2C_CR1_PE) == 0UL);
    }
    return false;
}

/**
 * @brief Transaction complete callback for I2C transfer initiated by calling
 *        HAL_I2C_Master_Transmit_IT().
 *        This is an override of the STM32 "weak" function of the same name.
 * 
 * @param [in] hi2c  I2C handle of the I2c that is expecting the interrupt
 */
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2c i2c = GetI2cFromHandle(hi2c);
    if (!i2c) return;
    i2c->state_machine.state = I2C_STATE_DONE;
}

/**
 * @brief Transaction complete callback for I2C transfer initiated by calling
 *        HAL_I2C_Master_Receive_IT().
 *        This is an override of the STM32 "weak" function of the same name.
 * 
 * @param [in] hi2c  I2C handle of the I2c that is expecting the interrupt
 */
void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2c i2c = GetI2cFromHandle(hi2c);
    if (!i2c) return;
    i2c->state_machine.state = I2C_STATE_DONE;
}

/**
 * @brief Transaction complete callback for I2C transfer initiated by calling
 *        HAL_I2C_Mem_Write_IT().
 *        This is an override of the STM32 "weak" function of the same name.
 * 
 * @param [in] hi2c  I2C handle of the I2c that is expecting the interrupt
 */
void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2c i2c = GetI2cFromHandle(hi2c);
    if (!i2c) return;
    i2c->state_machine.state = I2C_STATE_DONE;
}

/**
 * @brief Transaction complete callback for I2C transfer initiated by calling
 *        HAL_I2C_Mem_Read_IT().
 *        This is an override of the STM32 "weak" function of the same name.
 * 
 * @param [in] hi2c  I2C handle of the I2c that is expecting the interrupt
 */
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c) {
    I2c i2c = GetI2cFromHandle(hi2c);
    if (!i2c) return;
    i2c->state_machine.state = I2C_STATE_DONE;
}

/**
 * @brief Transaction error callback for I2C transfer.  All transfers, regardless
 *        of I2cXferMode use this error callback.
 *        This is an override of the STM32 "weak" function of the same name.
 * 
 * @param [in] hi2c  I2C handle of the I2c that is expecting the interrupt
 */
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
    I2c i2c = GetI2cFromHandle(hi2c);
    if (!i2c) return;
    i2c->state_machine.pending_error = MHAL_STATUS_ERROR_IO;
    i2c->state_machine.state = I2C_STATE_ERROR;
}

mHalStatus mHalI2c_SetInterruptPriority(I2c i2c, mHalInterruptPriority prior) {
    if (!i2c) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }
    IRQn_Type irqn = GetI2cIRQn(i2c->index);
    HAL_NVIC_SetPriority(irqn, prior, INTERRUPT_SUBPRIORITY_NOT_USED);

    irqn = GetI2cErrorIRQn(i2c->index);
    HAL_NVIC_SetPriority(irqn, prior, INTERRUPT_SUBPRIORITY_NOT_USED);
    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_Init(I2c* pI2c, I2cConfig* config) {
    if (config->index >= NUM_I2C) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // init GPIO
    mHalStatus gpio_status = mHalGpio_Init(config->gpio.scl, &config->gpio.scl_config);
    if (gpio_status != MHAL_STATUS_OK) {
        return gpio_status;
    }
    gpio_status = mHalGpio_Init(config->gpio.sda, &config->gpio.sda_config);
    if (gpio_status != MHAL_STATUS_OK) {
        return gpio_status;
    }

    I2c i2c = calloc(1, sizeof(I2c_t));
    if (!i2c) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    // set up instance
    i2c->hi2c.Instance = GetI2cBase(config->index);
    if (!i2c->hi2c.Instance) {
        free(i2c);
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    // enable I2C peripheal clock
    switch (config->index) {
        case I2C_INDEX_1:
            __HAL_RCC_I2C1_CLK_ENABLE();
            break;
#ifndef STM32L432xx
        case I2C_INDEX_2:
            __HAL_RCC_I2C2_CLK_ENABLE();
            break;
#endif // !STM32L432xx
#ifdef STM32L432xx
        case I2C_INDEX_3:
            __HAL_RCC_I2C3_CLK_ENABLE();
            break;
#endif // STM32L432xx
#if defined(STM32G474xx) || defined(STM32L496xx)
        case I2C_INDEX_3:
            __HAL_RCC_I2C3_CLK_ENABLE();
            break;
        case I2C_INDEX_4:
            __HAL_RCC_I2C4_CLK_ENABLE();
            break;
#endif // STM32G474xx || STM32L496xx
        default:
            break;
    }

    // initialize interrupt
    IRQn_Type irq = GetI2cIRQn(config->index);
    if (!irq) {
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    _mhal_checkInterruptPriority(irq);
    HAL_NVIC_EnableIRQ(irq);

    // initialize error interrupt
    irq = GetI2cErrorIRQn(config->index);
    if (!irq) {
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    _mhal_checkInterruptPriority(irq);
    HAL_NVIC_EnableIRQ(irq);

    // setup a dummy configuration so that we can enable the filters
    i2c->hi2c.Init.Timing           = I2C_TIMING_REG_STD;
    i2c->hi2c.Init.AddressingMode   = I2C_ADDRESSINGMODE_7BIT;
    i2c->hi2c.Init.DualAddressMode  = I2C_DUALADDRESS_DISABLE;
    i2c->hi2c.Init.OwnAddress1      = 0;
    i2c->hi2c.Init.OwnAddress2      = 0;
    i2c->hi2c.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    i2c->hi2c.Init.GeneralCallMode  = I2C_GENERALCALL_DISABLE;
    i2c->hi2c.Init.NoStretchMode    = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&i2c->hi2c) != HAL_OK) {
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    if (HAL_I2CEx_ConfigAnalogFilter(&i2c->hi2c, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        free(i2c);
        return MHAL_STATUS_ERROR;
    }
    if (HAL_I2CEx_ConfigDigitalFilter(&i2c->hi2c, 0) != HAL_OK)
    {
        free(i2c);
        return MHAL_STATUS_ERROR;
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

    __HAL_I2C_DISABLE(&i2c->hi2c);
    i2c->index = config->index;
    i2c_handle_table[config->index] = i2c;

    *pI2c = i2c;

    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_Xfer(I2c i2c, I2cXfer* xfer, uint8_t* buffer, size_t size) {
    if (!xfer || !buffer || !size) return MHAL_STATUS_ERROR_INVALID_PARAM;

    if (!i2c || !i2c->hi2c.Instance) return MHAL_STATUS_ERROR_INSTANCE;
    if (i2c->state_machine.state != I2C_STATE_OK) return MHAL_STATUS_BUSY;

    switch (xfer->mode) {
        case I2C_MODE_STANDARD:
            i2c->hi2c.Init.Timing = I2C_TIMING_REG_STD;
            break;
        case I2C_MODE_FAST:
            i2c->hi2c.Init.Timing = I2C_TIMING_REG_FAST;
            break;
        case I2C_MODE_FAST_PLUS:
            i2c->hi2c.Init.Timing = I2C_TIMING_REG_FAST_PLUS;
            break;
        default:
            return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    i2c->hi2c.Init.AddressingMode = (xfer->address_mode == I2C_ADDRESSING_MODE_10BITS) ?
        I2C_ADDRESSINGMODE_10BIT : I2C_ADDRESSINGMODE_7BIT;
    i2c->hi2c.Init.DualAddressMode       = I2C_DUALADDRESS_DISABLE;
    i2c->hi2c.Init.OwnAddress1           = 0;
    i2c->hi2c.Init.OwnAddress2           = 0;
    i2c->hi2c.Init.OwnAddress2Masks      = I2C_OA2_NOMASK;
    i2c->hi2c.Init.GeneralCallMode       = I2C_GENERALCALL_DISABLE;
    i2c->hi2c.Init.NoStretchMode         = I2C_NOSTRETCH_DISABLE;
    i2c->state_machine.xfer.mode         = xfer->mode;
    i2c->state_machine.xfer.address      = (xfer->address << 1);
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

    if (HAL_I2C_Init(&i2c->hi2c) != HAL_OK) {
        return MHAL_STATUS_ERROR;
    }
    if (xfer->mode == I2C_MODE_FAST_PLUS) {
        __HAL_SYSCFG_FASTMODEPLUS_ENABLE(fmp_flag[i2c->index]);
    }
    __HAL_I2C_DISABLE(&i2c->hi2c);

    i2c->state_machine.state = I2C_STATE_PENDING;
    return MHAL_STATUS_OK;
}

mHalStatus mHalI2c_StateMachine(I2c i2c) {
    mHalStatus retval = MHAL_STATUS_ERROR;
    if (!i2c || !i2c->hi2c.Instance) return MHAL_STATUS_ERROR_INSTANCE;

    switch (i2c->state_machine.state) {
        case I2C_STATE_OK:
            retval = MHAL_STATUS_OK;
            break;
        case I2C_STATE_PENDING:
            if (I2c_IsBusFree(i2c)) {
                __HAL_I2C_ENABLE(&i2c->hi2c);
                HAL_StatusTypeDef status;
                if (i2c->state_machine.xfer.write) {    // write xfer
                    if (i2c->state_machine.xfer.xfer_mode == I2C_XFER_MODE_MEMORY) {
                        status = HAL_I2C_Mem_Write_IT(&i2c->hi2c,
                            i2c->state_machine.xfer.address,
                            i2c->state_machine.xfer.reg,
                            i2c->state_machine.xfer.reg_size,
                            i2c->state_machine.xfer.buffer,
                            i2c->state_machine.xfer.buffer_size);
                    } else {                               // I2C_XFER_MODE_STREAM
                        status = HAL_I2C_Master_Transmit_IT(&i2c->hi2c,
                            i2c->state_machine.xfer.address,
                            i2c->state_machine.xfer.buffer,
                            i2c->state_machine.xfer.buffer_size);
                    }
                } else {                                 // read xfer
                    if (i2c->state_machine.xfer.xfer_mode == I2C_XFER_MODE_MEMORY) {
                        status = HAL_I2C_Mem_Read_IT(&i2c->hi2c,
                            i2c->state_machine.xfer.address,
                            i2c->state_machine.xfer.reg,
                            i2c->state_machine.xfer.reg_size,
                            i2c->state_machine.xfer.buffer,
                            i2c->state_machine.xfer.buffer_size);
                    } else {                               // I2C_XFER_MODE_STREAM
                        status = HAL_I2C_Master_Receive_IT(&i2c->hi2c,
                            i2c->state_machine.xfer.address,
                            i2c->state_machine.xfer.buffer,
                            i2c->state_machine.xfer.buffer_size);
                    }
                }
                if (status != HAL_OK) {
                    i2c->state_machine.pending_error = MHAL_STATUS_ERROR_IO;
                    i2c->state_machine.state = I2C_STATE_ERROR;
                } else {
                    // wait for interrupt
                    mHalTimer_StartTimerObj(&i2c->state_machine.timer);
                    i2c->state_machine.state = I2C_STATE_XFER;
                }
            }
            retval = MHAL_STATUS_BUSY;
            break;
        case I2C_STATE_XFER:
            if (mHalTimer_TimerObjElapsed(&i2c->state_machine.timer, I2C_XFER_TIMEOUT_US)) {
                HAL_I2C_Master_Abort_IT(&i2c->hi2c, i2c->state_machine.xfer.address);
                i2c->state_machine.pending_error = MHAL_STATUS_ERROR_TIMEOUT;
                i2c->state_machine.state = I2C_STATE_ERROR;
            }
            retval = MHAL_STATUS_BUSY;
            break;
        case I2C_STATE_DONE:
            if (i2c->state_machine.xfer.mode == I2C_MODE_FAST_PLUS) {
                __HAL_SYSCFG_FASTMODEPLUS_DISABLE(fmp_flag[i2c->index]);
            }
            __HAL_I2C_DISABLE(&i2c->hi2c);
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
            if (i2c->state_machine.xfer.mode == I2C_MODE_FAST_PLUS) {
                __HAL_SYSCFG_FASTMODEPLUS_DISABLE(fmp_flag[i2c->index]);
            }
            __HAL_I2C_DISABLE(&i2c->hi2c);
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

/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief This function handles I2C1 event interrupt.
  */
void I2C1_EV_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_1]) {
        HAL_I2C_EV_IRQHandler(&i2c_handle_table[I2C_INDEX_1]->hi2c);
    }
}

/**
  * @brief This function handles I2C1 error interrupt.
  */
void I2C1_ER_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_1]) {
        HAL_I2C_ER_IRQHandler(&i2c_handle_table[I2C_INDEX_1]->hi2c);
    }
}

#ifndef STM32L432xx
/**
  * @brief This function handles I2C2 event interrupt.
  */
void I2C2_EV_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_2]) {
        HAL_I2C_EV_IRQHandler(&i2c_handle_table[I2C_INDEX_2]->hi2c);
    }
}

/**
  * @brief This function handles I2C2 error interrupt.
  */
void I2C2_ER_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_2]) {
        HAL_I2C_ER_IRQHandler(&i2c_handle_table[I2C_INDEX_2]->hi2c);
    }
}
#endif // !STM32L432xx

#if defined(STM32G474xx)
/**
  * @brief This function handles I2C3 event interrupt.
  */
void I2C3_EV_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_3]) {
        HAL_I2C_EV_IRQHandler(&i2c_handle_table[I2C_INDEX_3]->hi2c);
    }
}

/**
  * @brief This function handles I2C3 error interrupt.
  */
void I2C3_ER_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_3]) {
        HAL_I2C_ER_IRQHandler(&i2c_handle_table[I2C_INDEX_3]->hi2c);
    }
}

/**
  * @brief This function handles I2C4 event interrupt.
  */
void I2C4_EV_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_4]) {
        HAL_I2C_EV_IRQHandler(&i2c_handle_table[I2C_INDEX_4]->hi2c);
    }
}

/**
  * @brief This function handles I2C4 error interrupt.
  */
void I2C4_ER_IRQHandler(void) {
    if (i2c_handle_table[I2C_INDEX_4]) {
        HAL_I2C_ER_IRQHandler(&i2c_handle_table[I2C_INDEX_4]->hi2c);
    }
}
#endif // defined(STM32G474xx)
