/**
 * @file mhal_uart.h
 * 
 * @brief UART HAL implementation for monkeyHAL
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#ifndef __MHAL_UART_H__
#define __MHAL_UART_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <termios.h>

#include "mhal_status.h"

// Forward declaration of Uart type
typedef struct _uart_t uart_t;
typedef uart_t* Uart;

// NOTE: Actual supported baud rates are platform dependent.
typedef enum {
    UART_BAUD_RATE_50      = B50,
    UART_BAUD_RATE_75      = B75,
    UART_BAUD_RATE_110     = B110,
    UART_BAUD_RATE_134     = B134,
    UART_BAUD_RATE_150     = B150,
    UART_BAUD_RATE_200     = B200,
    UART_BAUD_RATE_300     = B300,
    UART_BAUD_RATE_600     = B600,
    UART_BAUD_RATE_1200    = B1200,
    UART_BAUD_RATE_1800    = B1800,
    UART_BAUD_RATE_2400    = B2400,
    UART_BAUD_RATE_4800    = B4800,
    UART_BAUD_RATE_9600    = B9600,
    UART_BAUD_RATE_19200   = B19200,
    UART_BAUD_RATE_38400   = B38400,
    UART_BAUD_RATE_57600   = B57600,
    UART_BAUD_RATE_115200  = B115200,
    UART_BAUD_RATE_230400  = B230400,
#ifdef LINUX
    UART_BAUD_RATE_460800  = B460800,
    UART_BAUD_RATE_921600  = B921600,
    UART_BAUD_RATE_1000000 = B1000000,
    UART_BAUD_RATE_1500000 = B1500000,
    UART_BAUD_RATE_2000000 = B2000000,
    UART_BAUD_RATE_2500000 = B2500000,
    UART_BAUD_RATE_3000000 = B3000000,
    UART_BAUD_RATE_3500000 = B3500000,
    UART_BAUD_RATE_4000000 = B4000000,
#endif // LINUX
} UartBaudRate;

typedef enum {
    UART_DATA_BITS_5 = CS5,
    UART_DATA_BITS_6 = CS6,
    UART_DATA_BITS_7 = CS7,
    UART_DATA_BITS_8 = CS8,
} UartDataBits;  // use for c_cflag

typedef enum {
    UART_STOP_BITS_1 = 0,      // 1 stop bit
    UART_STOP_BITS_2 = CSTOPB, // 2 stop bits
} UartStopBits;  // use for c_cflag

typedef enum {
    UART_PARITY_NONE = 0,
    UART_PARITY_ODD  = PARENB | PARODD,
    UART_PARITY_EVEN = PARENB,
} UartParity;  // use for c_cflag

// Uart configuration structure
typedef struct {
    UartBaudRate baud;     // Baud rate for the UART communication
    UartDataBits data_bits; // Number of data bits (5, 6, 7, or 8)
    UartStopBits stop_bits; // Number of stop bits (1 or 2)
    UartParity parity; // Parity setting (none, odd, or even)
} UartConfig;

/**
 * @brief Initialize Uart instance.  This function sets all the UART parameters
 *        and opens the UART port for communication.
 * 
 * @param  pUart     Pointer to the Uart instance to initialize
 * @param  port      The UART port to open (e.g., "/dev/ttyUSB0")
 * @param  config    Pointer to the UartConfig structure containing UART settings
 * @param  ring_buffer_size Size of the ring buffers to create for TX and RX
 *
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 * 
 * @note This function allocates memory for the Uart instance and its ring buffers.
 *       It should be called during system initialization.
 */
extern mHalStatus mHalUart_Init(Uart* pUart, const char* port, UartConfig* config, size_t ring_buffer_size);

/**
 * @brief Deinitialize Uart instance. This function closes the UART port,
 *        destroys the ring buffers, and frees the allocated memory for the Uart instance.
 * 
 * @param  pUart Pointer to the Uart instance to deinitialize
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHALerror
 * 
 * @note This function should be called when the UART is no longer needed, typically during system shutdown.
 */
extern mHalStatus mHalUart_Deinit(Uart* pUart);

/**
 * @brief Set loopback mode for the UART instance.
 * 
 * @param uart Uart instance
 * @param enable True to enable loopback mode, false to disable
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalUart_SetLoopback(Uart uart, bool enable);

/**
 * @brief Send data through the UART port.
 * 
 * @param uart Uart instance
 * @param data Pointer to the data buffer to send
 * @param size Size of the data to send
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 * 
 * @note This function adds the data to the TX ring buffer. The actual sending of data
 *       is handled in the UART state machine. This allows for non-blocking operation.
 */
extern mHalStatus mHalUart_SendData(Uart uart, const uint8_t* data, size_t size);

/**
 * @brief Get data from the UART RX ring buffer.
 * 
 * @param uart Uart instance
 * @param data Pointer to the buffer to store received data
 * @param size Size of the data to get
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 * 
 * @note This function retrieves data from the RX ring buffer. It does not block and will return
 *       immediately if no data is available.
 */
extern mHalStatus mHalUart_GetData(Uart uart, uint8_t* data, size_t size);

/**
 * @brief Get a single byte from the UART RX ring buffer.
 * 
 * @param uart Uart instance
 * @param data Pointer to store the received byte
 * 
 * @return MHAL_STATUS_OK if successful, otherwise a monkeyHAL error
 */
extern mHalStatus mHalUart_GetByte(Uart uart, uint8_t* data);

/**
 * @brief State machine for UART operations. This function processes the UART state machine,
 *        reading data from the UART port and managing the TX and RX buffers.
 * 
 * @param uart Uart instance
 * 
 * @return HAL_STATUS_OK if successful, otherwise a monkeyHALl error
 */
extern mHalStatus mHalUart_StateMachine(Uart uart);

#endif // __MHAL_UART_H__
