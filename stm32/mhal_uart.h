/**
 * @file mhal_uart.h
 * 
 * @brief UART abstraction layer for mHAL 
 *        MCUs supported:
 *        - STM32F3xx
 *        - STM32G4xx
 *        - STM32L4xx
 * 
 *        Contained in this HAL is also a UART state machine.  The state
 *        machine is exercised by the background task (i.e. main while loop).
 *        Given the 1:1 nature of a UART (that is, only one device is connected
 *        to a UART), the driver for any connected device will have it's own
 *        state machine to interact with the UART.
 *        
 *        TX is handled as follows:
 *          1.  The device's state machine will call HalUart_SendData() to
 *              send data to the device.
 *          2.  The call to HalUart_SendData() actually does not send the data
 *              but rather queues data in a TX ring buffer.
 *          3.  The UART state machine will consume data from the TX ring buffer
 *              1-byte per execution of the state machine until all data is
 *              consumed.
 *          4.  While data is transmitted, the state machine is in a busy state.
 *              The state machine remains in this state until the transfer
 *              is complete, at which time an interrupt is generated to put the
 *              state machine into a done state.
 * 
 *        RX is handled as follows:
 *          1.  RX flow is governed strictly by interrupts.  Upon init of the
 *              UART, an RX interrupt is set up to trigger upon receiving 1
 *              byte of data.
 *          2.  When data is received, the UART ISR dumps data into an RX ring
 *              buffer and sets up the next interrupt (again, 1-byte trigger).
 *          3.  The devices state machine will poll the uart for a byte of 
 *              data by calling HalUart_GetByte().  The device state machine is
 *              responsible for collecting data, storing it, and parsing it
 *              accordingly to process a full packet of data.
 * 
 *        Current ring buffer size for TX and RX is UART_RING_BUFFER_SZ.  We
 *        may need to adjust this size depending on the device.
 * 
 *        If data transfer sizes are fixed, it may be more optimal to utilize
 *        DMA instead of 1-byte transactions.  We will implement this as the
 *        need arises.
 *
 * @warning In the STM32, the "UART" and "USART" utilize the same hardware
 *          peripheral.  They are cast as USART_TypeDef* but you can utilize
 *          either the STM32 UART interface or the USART interface.  We are
 *          strictly using the UART interface.
 *          To add more confusiton, the STM32 HAL has a mix of USARTs and UARTs
 *          or could be all USARTs depending on the MCU.  Traditionally, the
 *          first 3 UARTS were actually USARTs and any additionally UARTs were
 *          UARTs.  We believe that to keep backwards compatibility with older
 *          versions of STM32 HAL, UART4 and UART5 are named as such even
 *          though they may actually be USARTs.  There may be issues when using
 *          UART4 and UART5 that we haven't caught yet.  
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 *
 * @todo add DMA, if needed
 * @todo add more baudrates, if needed
 * @todo add any cleanup to be done for UART_STATE_ERROR in UART state machine
 * @todo determine if UART_TX_TIMEOUT_USEC needs to be adjusted
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 */

#ifndef __MHAL_UART_H__
#define __MHAL_UART_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "mhal_gpio.h"
#include "mhal_status.h"
#include "mhal_interrupt.h"

typedef struct _uart_t uart_t;
typedef uart_t* Uart;

typedef enum {
    UART_BAUD_9600   = 9600,
    UART_BAUD_19200  = 19200,
    UART_BAUD_38400  = 38400,
    UART_BAUD_57600  = 57600,
    UART_BAUD_115200 = 115200,
    UART_BAUD_230400 = 230400,
    UART_BAUD_460800 = 460800,
    UART_BAUD_576000 = 576000,
    UART_BAUD_921600 = 921600,
} UartBaud;

typedef enum {
    UART_DATA_BITS_8,
    UART_DATA_BITS_9,
} UartDataBits;

typedef enum {
    UART_STOP_BITS_0_5,
    UART_STOP_BITS_1,
    UART_STOP_BITS_1_5,
    UART_STOP_BITS_2,
} UartStopBits;

typedef enum {
    UART_PARITY_BITS_NONE,
    UART_PARITY_BITS_EVEN,
    UART_PARITY_BITS_ODD,
} UartParityBits;

typedef enum {
    UART_XFER_HALF_DUPLEX_RX,
    UART_XFER_HALF_DUPLEX_TX,
    UART_XFER_FULL_DUPLEX,
} UartXfer;

typedef enum {
    UART_HW_FLOW_CTRL_NONE,
    UART_HW_FLOW_CTRL_RTS,
    UART_HW_FLOW_CTRL_CTS,
    UART_HW_FLOW_CTRL_RTS_CTS,
} UartHwFlowCtrl;

typedef enum {
    UART_INDEX_1 = 0,
    UART_INDEX_2 = 1,
    UART_INDEX_3 = 2,
    UART_INDEX_4 = 3,
    UART_INDEX_5 = 4,
    NUM_UARTS,
} UartIndex;

typedef struct {
    struct {
        Gpio tx;
        GpioConfig tx_config;
        Gpio rx;
        GpioConfig rx_config;
    } gpio;
    UartIndex index;
    UartBaud baud;
    UartDataBits data_bits;
    UartStopBits stop_bits;
    UartParityBits parity_bits;
    UartXfer xfer;
    UartHwFlowCtrl flow_ctrl;
    size_t ring_buffer_size;
} UartConfig;

/**
 * @brief Set interrupt priority.
 *
 * @param [in] uart  Uart instance
 * @param [in] prior interrupt priority level
 *
 * @return MHAL_STATUS_OK if successfully initialized, otherwise mHAL error
 */
extern mHalStatus mHalUart_SetInterruptPriority(Uart uart, HalInterruptPriority prior);

/**
 * @brief Initialize Uart instance.  This function sets all the UART parameters
 *        and sets up the RX interrupt.
 * 
 * @param [in] p            pointer to Uart instance
 * @param [in] config       pointer to UartConfig 
 * 
 * @return MHAL_STATUS_OK if successfully initialized, otherwise mHAL error
 */
extern mHalStatus mHalUart_Init(Uart* pUart, UartConfig* config);

/**
 * @brief Queue TX data in UART TX ring buffer
 * 
 * @param [in] uart   Uart instance
 * @param [in] buffer  pointer to buffer
 * @param [in] size    size of the buffer
 * 
 * @return mHalStatus  MHAL_STATUS_OK if successfully queued, otherwise mHAL error
 */
extern mHalStatus mHalUart_SendData(Uart uart, const uint8_t* buffer, size_t size);

/**
 * @brief Get byte from UART RX ring buffer
 * 
 * @param [in]  uart  Uart instance
 * @param [out] data   data buffer to store RX data
 * 
 * @return MHAL_STATUS_OK if data is valid, MHAL_STATUS_DATA_INVALID if
 *         no data available (i.e. ring buffer is empty), otherwise
 *         mHAL error
 */
extern mHalStatus mHalUart_GetByte(Uart uart, uint8_t* data);

/**
 * @brief Enable/Disable loopback mode.
 *        When loopback mode is enabled, all data received in RX will be looped
 *        to TX
 * 
 * @param [in] uart   Uart instance
 * @param [in] enable  true to enable loopback mode, false to disable
 * 
 * @return MHAL_STATUS_OK if command accepted, otherwise mHAL error
 */
extern mHalStatus mHalUart_LoopbackCtrl(Uart uart, bool enable);

/**
 * @brief Sets the baud rate
 * 
 * @param [in] uart  Uart instance
 * @param [in] baud  desired baud rate
 * 
 * @return MHAL_STATUS_OK if sucessfully set baud rate, otherwise mHAL error
 * 
 * @note This function only needs to be called if HalUart_Init() has been 
 *       called and the baud wants to be changed.
 */
extern mHalStatus mHalUart_SetBaud(Uart uart, UartBaud baud);

/**
 * @brief UART state machine (see file comments for detailed description)
 * 
 * @param [in] uart  Uart instance
 * 
 * @return MHAL_STATUS_OK if state machine executed properly, otherwise mHAL error
 */
extern mHalStatus mHalUart_StateMachine(Uart uart);

#endif // __MHAL_UART_H__
