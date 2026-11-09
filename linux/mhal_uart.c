/**
 * @file mhal_uart.c
 * 
 * @brief UART HAL implementation for monkeyHAL.
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 * 
 */

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mhal_uart.h"
#include "ring_buffer.h"

#ifdef DEBUG_IO
#include <errno.h>
#include <stdio.h>
#endif // DEBUG_IO

#define UART_PORT_MAX_LENGTH_BYTES 64

typedef enum {
    UART_STATE_OK,
    UART_STATE_BUSY,
    UART_STATE_DONE,
    UART_STATE_ERROR,
} UartState;

typedef struct {
    UartState state; // Current state of the UART operation
    mHalStatus pending_error;
    uint32_t error_count;
    bool loopback;
} UartStateMachine;

typedef struct _uart_t {
    char port[UART_PORT_MAX_LENGTH_BYTES]; // Port name (e.g., "/dev/ttyUSB0")
    int fd; // File descriptor for the UART port
    RingBuffer tx_buffer;
    RingBuffer rx_buffer;
    size_t ring_buffer_size; // Size of the ring buffers
    UartStateMachine state_machine;
} uart_t;


mHalStatus mHalUart_Init(Uart* pUart, const char* port, UartConfig* config, size_t ring_buffer_size) {
    if (!port || !pUart || !config || ring_buffer_size == 0) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Allocate memory for the UART instance
    Uart new_uart = calloc(1, sizeof(uart_t));
    if (!new_uart) {
        return MHAL_STATUS_ERROR;
    }

    // Initialize UART properties
    strncpy(new_uart->port, port, sizeof(new_uart->port) - 1);
    new_uart->port[sizeof(new_uart->port) - 1] = '\0';
    new_uart->fd = -1; // File descriptor not opened yet

    // Create ring buffers
    mHalStatus status = RingBuffer_Create(&new_uart->tx_buffer, ring_buffer_size);
    if (status != MHAL_STATUS_OK) {
        free(new_uart);
        return status;
    }
    
    status = RingBuffer_Create(&new_uart->rx_buffer, ring_buffer_size);
    if (status != MHAL_STATUS_OK) {
        RingBuffer_Destroy(&new_uart->tx_buffer);
        free(new_uart);
        return status;
    }
    new_uart->ring_buffer_size = ring_buffer_size;

    // Initialize state machine
    new_uart->state_machine.state = UART_STATE_OK;
    new_uart->state_machine.pending_error = MHAL_STATUS_OK;
    new_uart->state_machine.error_count = 0;
    new_uart->state_machine.loopback = false;

    // open the UART port
    new_uart->fd = open(new_uart->port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (new_uart->fd < 0) {
        RingBuffer_Destroy(&new_uart->tx_buffer);
        RingBuffer_Destroy(&new_uart->rx_buffer);
        free(new_uart);
        return MHAL_STATUS_ERROR_IO;
    }

    // Configure the UART port with the provided settings
    struct termios options;
    if (tcgetattr(new_uart->fd, &options) < 0) {
        close(new_uart->fd);
        RingBuffer_Destroy(&new_uart->tx_buffer);
        RingBuffer_Destroy(&new_uart->rx_buffer);
        free(new_uart);
        return MHAL_STATUS_ERROR_IO;
    }

    // Configure input modes
    options.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    options.c_iflag |= (IXOFF | IXANY); // Enable software flow control

    // Configure output modes
    options.c_oflag &= ~OPOST; // Disable output processing

    // Configure control modes
    options.c_cflag &= ~(CSIZE | PARENB | CSTOPB | CRTSCTS);
    options.c_cflag |= (config->data_bits | config->parity |
                        (config->stop_bits == UART_STOP_BITS_2 ? CSTOPB : 0));
    options.c_cflag |= (CLOCAL | CREAD); // Ignore modem control lines and enable receiver

    // Configure local modes
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG | IEXTEN); // Disable canonical mode, echo, and extended functions
    options.c_lflag |= (ISIG | IEXTEN); // Enable signal handling and extended functions

    // Set the baud rates
    cfsetispeed(&options, config->baud);
    cfsetospeed(&options, config->baud);

    // Apply the settings
    if (tcsetattr(new_uart->fd, TCSANOW, &options) < 0) {
        close(new_uart->fd);
        RingBuffer_Destroy(&new_uart->tx_buffer);
        RingBuffer_Destroy(&new_uart->rx_buffer);
        free(new_uart);
        return MHAL_STATUS_ERROR_IO;
    }

    // enable interrupts for the UART port
    if (fcntl(new_uart->fd, F_SETFL, O_NONBLOCK) < 0) {
        close(new_uart->fd);
        RingBuffer_Destroy(&new_uart->tx_buffer);
        RingBuffer_Destroy(&new_uart->rx_buffer);
        free(new_uart);
        return MHAL_STATUS_ERROR_IO;
    }


    *pUart = new_uart;

    printf("<mHalUart> Initialized UART on port %s with baud rate %d\n",
           new_uart->port, config->baud);
    
    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_Deinit(Uart* pUart) {
    if (!pUart || !*pUart) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    Uart uart_instance = *pUart;

    // Close the UART port
    if (uart_instance->fd >= 0) {
        close(uart_instance->fd);
    }

    // Destroy the ring buffers
    mHalStatus status = RingBuffer_Destroy(&uart_instance->tx_buffer);
    if (status != MHAL_STATUS_OK) {
        return status;
    }
    
    status = RingBuffer_Destroy(&uart_instance->rx_buffer);
    if (status != MHAL_STATUS_OK) {
        return status;
    }

    // Free the UART instance
    free(uart_instance);
    *pUart = NULL;

    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_SetLoopback(Uart uart, bool enable) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    uart->state_machine.loopback = enable;
    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_SendData(Uart uart, const uint8_t* data, size_t size) {
    if (!uart || !data || size == 0) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    // Add data to the TX ring buffer
    return (RingBuffer_AddData(uart->tx_buffer, data, size));
}

mHalStatus mHalUart_GetData(Uart uart, uint8_t* data, size_t size) {
    if (!uart || !data || size == 0) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    // Get data from the RX ring buffer
    return RingBuffer_GetData(uart->rx_buffer, data, size);
}

mHalStatus mHalUart_GetByte(Uart uart, uint8_t* data) {
    if (!uart || !data) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }
    // Get a single byte from the RX ring buffer
    return RingBuffer_GetData(uart->rx_buffer, data, 1);
}

mHalStatus mHalUart_StateMachine(Uart uart) {
    mHalStatus retval = MHAL_STATUS_ERROR;

    if (!uart) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    uint8_t data[uart->ring_buffer_size/2];
    uint8_t tx_data;
    ssize_t bytes_read;
    switch (uart->state_machine.state) {
        case UART_STATE_OK:
            // Read data from UART
            bytes_read = read(uart->fd, data, uart->ring_buffer_size/2);
            if (bytes_read <= 0) {
                // print errno if read failed
#ifdef DEBUG_IO
                if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                    printf("<mHalUartT> Read failed: %s\n", strerror(errno));
                }
#endif // DEBUG_IO
                // No data available
                retval = MHAL_STATUS_OK;
                break;
            } else if (uart->state_machine.loopback) {
                // If loopback is enabled, write the data back to the TX buffer
                mHalStatus status = RingBuffer_AddData(uart->tx_buffer, data, bytes_read);
                if (status != MHAL_STATUS_OK) {
                    uart->state_machine.pending_error = status;
                    uart->state_machine.state = UART_STATE_ERROR;
                    retval = MHAL_STATUS_BUSY;
                    break;
                }
            } else {
                // Add data to RX buffer
#ifdef DEBUG_IO
                printf("<mHalUart> Received %ld bytes: ", bytes_read);
                for (ssize_t i = 0; i < bytes_read; i++) {
                    printf("0x%02X ", data[i]);
                }
                printf("\n");
#endif // DEBUG_IO
                mHalStatus status = RingBuffer_AddData(uart->rx_buffer, data, bytes_read);
                if (status != MHAL_STATUS_OK) {
                    uart->state_machine.pending_error = status;
                    uart->state_machine.state = UART_STATE_ERROR;
                    retval = MHAL_STATUS_BUSY;
                    break;
                }
            }

            // Process TX buffer
            while (!RingBuffer_IsEmpty(uart->tx_buffer)) {
                mHalStatus status = RingBuffer_GetData(uart->tx_buffer, &tx_data, 1);
                if (status != MHAL_STATUS_OK) {
                    uart->state_machine.pending_error = status;
                    uart->state_machine.state = UART_STATE_ERROR;
                    retval = MHAL_STATUS_BUSY;
                    break;
                }

                // Write data to UART
                write(uart->fd, &tx_data, 1);
            }
            retval = MHAL_STATUS_OK;
            break;
        case UART_STATE_ERROR:
            uart->state_machine.error_count++;
            uart->state_machine.state = UART_STATE_OK; // Reset state to OK after handling error
            retval = uart->state_machine.pending_error;
            uart->state_machine.pending_error = MHAL_STATUS_OK; // Reset pending error
            break;
        default:
            // should not get here
            uart->state_machine.state = UART_STATE_OK;
            uart->state_machine.pending_error = MHAL_STATUS_OK;
            break;
    }

    return retval;
}
