/**
 * @file mhal_uart.c
 * 
 * @copyright Copyright © 2026 Purple Monkey Software. All rights reserved.
 */

#if defined(STM32CubeF3)
#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_uart.h"
#elif defined(STM32CubeG4)
#include "stm32g4xx_hal.h"
#include "stm32g4xx_hal_uart.h"
#elif defined(STM32CubeL4)
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_uart.h"
#else
#error "MCU not supported!"
#endif

#include <stdint.h>
#include <stdbool.h>
#include "_mhal_internal.h"
#include "mhal_uart.h"
#include "mhal_timer.h"
#include "ring_buffer.h"

#define UART_TX_TIMEOUT_USEC (1000000)  // 1 sec

typedef enum {
    UART_STATE_OK,
    UART_STATE_BUSY,
    UART_STATE_DONE,
    UART_STATE_ERROR,
} UartState;

typedef struct {
    volatile UartState state;
    mHalTimerObj timer;
    mHalStatus pending_error;
    uint32_t error_count;
    bool loopback;
} UartStateMachine;

typedef struct _uart_t {
    UartIndex index;
    UART_HandleTypeDef hUart;
    RingBuffer tx_buffer;
    RingBuffer rx_buffer;
    uint8_t rx_recv_temp;
    bool tx_busy;
    UartStateMachine state_machine;
} uart_t;



/**
 * @brief Map mHAL data bits to hardware-specific data bits
 * 
 * @param [in] data  mHAL data bits
 * 
 * @return  hardware-specific data bits
 */
static uint32_t GetUartWordLength(UartDataBits data) {
    uint32_t retval = 0UL;

    switch (data) {
    case UART_DATA_BITS_8:
        retval = UART_WORDLENGTH_8B;
        break;
    case UART_DATA_BITS_9:
        retval = UART_WORDLENGTH_9B;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Map mHAL stop bits to hardware-specific stop bits
 * 
 * @param [in] stop  mHAL stop bits
 * 
 * @return hardware-specific stop bits
 */
static uint32_t GetUartStopBits(UartStopBits stop) {
    uint32_t retval = 0UL;

    switch (stop) {
    case UART_STOP_BITS_0_5:
        retval = UART_STOPBITS_0_5;
        break;
    case UART_STOP_BITS_1:
        retval = UART_STOPBITS_1;
        break;
    case UART_STOP_BITS_1_5:
        retval = UART_STOPBITS_1_5;
        break;
    case UART_STOP_BITS_2:
        retval = UART_STOPBITS_2;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Map mHAL parity to Shardware-specific parity
 * 
 * @param [in] parity  mHAL parity bits
 * 
 * @return hardware-specific parity bits
 */
static uint32_t GetUartParity(UartParityBits parity) {
    uint32_t retval = 0UL;

    switch (parity) {
    case UART_PARITY_BITS_NONE:
        retval = UART_PARITY_NONE;
        break;
    case UART_PARITY_BITS_EVEN:
        retval = UART_PARITY_EVEN;
        break;
    case UART_PARITY_BITS_ODD:
        retval = UART_PARITY_ODD;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Map mHAL transfer type to hardware-specific transfer type
 * 
 * @param [in] xfer  mHAL transfer type
 * 
 * @return hardware-specific transfer type
 */
static uint32_t GetUartXfer(UartXfer xfer) {
    uint32_t retval = 0UL;

    switch (xfer) {
    case UART_XFER_HALF_DUPLEX_RX:
        retval = UART_MODE_RX;
        break;
    case UART_XFER_HALF_DUPLEX_TX:
        retval = UART_MODE_TX;
        break;
    case UART_XFER_FULL_DUPLEX:
        retval = UART_MODE_TX_RX;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Map mHAL flow control to hardware-specific flow control type
 * 
 * @param [in] ctrl  mHAL flow control type
 * 
 * @return hardware-specific flow control type
 */
static uint32_t GetUartFlowCtrl(UartHwFlowCtrl ctrl) {
    uint32_t retval = 0UL;

    switch (ctrl) {
    case UART_HW_FLOW_CTRL_NONE:
        retval = UART_HWCONTROL_NONE;
        break;
    case UART_HW_FLOW_CTRL_RTS:
        retval = UART_HWCONTROL_RTS;
        break;
    case UART_HW_FLOW_CTRL_CTS:
        retval = UART_HW_FLOW_CTRL_CTS;
        break;
    case UART_HW_FLOW_CTRL_RTS_CTS:
        retval = UART_HWCONTROL_RTS_CTS;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Get hardware-specific UART peripheral
 * 
 * @param [in] index  index of which UART to get
 * 
 * @return pointer to UART peripheral
 */
static USART_TypeDef* GetUartBase(UartIndex index) {
    USART_TypeDef* retval = NULL;

    switch (index) {
    case UART_INDEX_1:
        retval = USART1;
        break;
    case UART_INDEX_2:
        retval = USART2;
        break;
    case UART_INDEX_3:
        retval = USART3;
        break;
    case UART_INDEX_4:
        retval = UART4;
        break;
    case UART_INDEX_5:
        retval = UART5;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Enable UART preipjeral clock
 * 
 * @param [in] index  index of which UART clock to enable
 */
static void UartClockEnable(UartIndex index) {
    switch (index) {
    case UART_INDEX_1:
    	__HAL_RCC_USART1_CLK_ENABLE();
        break;
    case UART_INDEX_2:
    	__HAL_RCC_USART2_CLK_ENABLE();
        break;
    case UART_INDEX_3:
    	__HAL_RCC_USART3_CLK_ENABLE();
        break;
    case UART_INDEX_4:
    	__HAL_RCC_UART4_CLK_ENABLE();
        break;
    case UART_INDEX_5:
    	__HAL_RCC_UART5_CLK_ENABLE();
        break;
    default:
        break;
    }
}

/**
 * @brief Get UART clock frequency
 * 
 * @param [in] index index of which UART to get
 * 
 * @return frequency of the clock driving the UART
 */
static uint32_t UartGetClockFreq(UartIndex index) {
    if (index == UART_INDEX_1) {
        return HAL_RCC_GetPCLK2Freq();
    }
    return HAL_RCC_GetPCLK1Freq();
}

/**
 * @brief Get UART interrupt request vector
 * 
 * @param [in] index  index of which UART to get
 * 
 * @return UART interrupt request vector
 */
static IRQn_Type GetUartIRQn(UartIndex index) {
    IRQn_Type retval = 0;

    switch (index) {
    case UART_INDEX_1:
        retval = USART1_IRQn;
        break;
    case UART_INDEX_2:
        retval = USART2_IRQn;
        break;
    case UART_INDEX_3:
        retval = USART3_IRQn;
        break;
    case UART_INDEX_4:
        retval = UART4_IRQn;
        break;
    case UART_INDEX_5:
        retval = UART5_IRQn;
        break;
    default:
        break;
    }

    return retval;
}

/**
 * @brief Check if UART is enabled
 * 
 * @param [in] pUart  pointer to UART
 * 
 * @return       true if enabled, otherwise false
 */
static bool IsUartEnabled(Uart uart) {
    if (!uart) {
        return false;
    }
    return (uart->hUart.Instance != NULL);
}

/**
 * @brief Get Uart instance from STM32 UART handle
 * 
 * @param [in] hUart  pointer to STM32 UART handle
 * 
 * @return       Uart instance
 */
static Uart* GetUartFromHandle(UART_HandleTypeDef* hUart) {
    Uart* retval = NULL;

    for (uint8_t index = 0; index < NUM_UARTS; index++) {
        if (hUart == &uart_inst[index].hUart) {
            retval = &uart_inst[index];
            break;
        }
    }
    return retval;
}

mHalStatus mHalUart_SetInterruptPriority(Uart uart, HalInterruptPriority prior) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }
    IRQn_Type irqn = GetUartIRQn(uart->index);
    HAL_NVIC_SetPriority(irqn, prior, INTERRUPT_SUBPRIORITY_NOT_USED);
    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_Init(Uart* pUart, UartConfig* config) {
    if (!pUart || !config) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    // init GPIO
    mHalStatus gpio_status = HalGpio_Init(config->gpio.tx, config->gpio.tx_config);
    if (gpio_status != MHAL_STATUS_OK) {
        return gpio_status;
    }
    gpio_status = HalGpio_Init(config->gpio.rx, config->gpio.rx_config);
    if (gpio_status != MHAL_STATUS_OK) {
        return gpio_status;
    }

    Uart uart = malloc(sizeof(uart_t));
    if (!uart) {
        *pUart = NULL;
        return MHAL_STATUS_ERROR;
    }
    UartClockEnable(config->index);
    uart->index                             = config->index;
    uart->hUart.Instance                    = GetUartBase(config->index);
    uart->hUart.Init.BaudRate               = config->baud;
    uart->hUart.Init.WordLength             = GetUartWordLength(config->data);
    uart->hUart.Init.StopBits               = GetUartStopBits(config->stop);
    uart->hUart.Init.Parity                 = GetUartParity(config->parity);
    uart->hUart.Init.Mode                   = GetUartXfer(config->xfer);
    uart->hUart.Init.HwFlowCtl              = GetUartFlowCtrl(config->ctrl);
    uart->hUart.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    uart->hUart.Init.OverSampling           = UART_OVERSAMPLING_16;
    uart->hUart.Init.OneBitSampling         = UART_ONE_BIT_SAMPLE_DISABLE;
// TODO check if we need the ClockPrescaler for STM32L496xx
//#if defined(STM32G474xx) || defined(STM32L496xx)
#if defined(STM32G474xx)
    uart->hUart.Init.ClockPrescaler         = UART_PRESCALER_DIV1;
#endif // STM32G474xx || STM32L496xx
    if (HAL_UART_Init(&uart->hUart) != HAL_OK) {
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return MHAL_STATUS_ERROR;
    }

    // set up ring buffers
    if (RingBuffer_Create(&uart->tx_buffer, ring_buffer_size) != MHAL_STATUS_OK) {
        uart->hUart.Instance = NULL;
        free(uart);
        return MHAL_STATUS_ERROR;
    }
    if (RingBuffer_Create(&uart->rx_buffer, ring_buffer_size) != HAL_STATUS_OK) {
        RingBuffer_Destroy(&uart->tx_buffer);
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return MHAL_STATUS_ERROR;
    }

// TODO check if we need the Fifo thresholds for STM32L496xx
//#if defined(STM32G474xx) || defined(STM32L496xx)
#if defined(STM32G474xx)
    if (HAL_UARTEx_SetTxFifoThreshold(&art->hUart, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK) {
        RingBuffer_Destroy(&uart->tx_buffer);
        RingBuffer_Destroy(&uart->rx_buffer);
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return HAL_STATUS_ERROR;
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&uart->hUart, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        RingBuffer_Destroy(&uart->tx_buffer);
        RingBuffer_Destroy(&uart->rx_buffer);
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return HAL_STATUS_ERROR;
    }
    if (HAL_UARTEx_DisableFifoMode(&uart->hUart) != HAL_OK)
    {
        RingBuffer_Destroy(&uart->tx_buffer);
        RingBuffer_Destroy(&uart->rx_buffer);
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return HAL_STATUS_ERROR;
    }
#endif // STM32G474xx || STM32L496xx

    // initialize UART state machine
    uart->state_machine.state = UART_STATE_OK;
    uart->state_machine.pending_error = HAL_STATUS_OK;

    // prioritize and enable interrupt
    IRQn_Type irqn = GetUartIRQn(uart->index);
    _mhal_checkInterruptPriority(irqn);
    HAL_NVIC_EnableIRQ(irqn);

    // set up receive interrupt
    __HAL_UART_CLEAR_IT(&uart->hUart, 0xFFFFFFFF); // write all 1's to clear interrupts
    if (HAL_UART_Receive_IT(&uart->hUart, &uart->rx_recv_temp, 1) != HAL_OK) {
        RingBuffer_Destroy(&uart->tx_buffer);
        RingBuffer_Destroy(&uart->rx_buffer);
        uart->hUart.Instance = NULL;
        free(uart);
        *pUart = NULL;
        return MHAL_STATUS_ERROR_IO;
    }

    *pUart = uart;
    return MHAL_STATUS_OK;
}

/**
 * @brief Callback function for the TX complete interrupt.  This is an override
 *        of the STM32 callback of the same name.
 * 
 * @param hUart  pointer to the STM32 UART handle
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *hUart) {
    Uart* pUart = GetUartFromHandle(hUart);

    pUart->state_machine.state = UART_STATE_DONE;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *hUart) {
    Uart* pUart = GetUartFromHandle(hUart);

    if (pUart) {
        RingBuffer_AddData(pUart->rx_buffer, &pUart->rx_recv_temp, 1);

        // set up next interrupt
        HAL_UART_Receive_IT(&pUart->hUart, &pUart->rx_recv_temp, 1);
    }
}

mHalStatus mHalUart_SendData(Uart uart, const uint8_t* buffer, size_t size) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    if (!IsUartEnabled(uart)) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    RingBuffer_AddData(uart->tx_buffer, (uint8_t*) buffer, size);

    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_GetByte(Uart uart, uint8_t* data) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }
    if (!data) {
        return MHAL_STATUS_ERROR_INVALID_PARAM;
    }

    if (!RingBuffer_IsEmpty(pUart->rx_buffer)) {
        RingBuffer_GetData(pUart->rx_buffer, data, 1);
        return MHAL_STATUS_OK;
    }

    return MHAL_STATUS_DATA_INVALID;
}

mHalStatus mHalUart_LoopbackCtrl(Uar uart, bool enable) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    if (!IsUartEnabled(uart)) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    uart->state_machine.loopback = enable;
    return MHAL_STATUS_OK;
}

/**
 * @brief Check if Uart has data to send
 * 
 * @param [in] uart  Uart instance
 * 
 * @return       true if there is data to send, otherwise false
 */
static bool UartReadyToSend(Uart uart) {
    if (!uart) {
        return false;
    }
    return (!uart->tx_busy && !RingBuffer_IsEmpty(uart->tx_buffer));
}

mHalStatus mHalUart_SetBaud(Uart uart, UartBaud baud) {
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    if (!IsUartEnabled(uart)) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    uint32_t clk = UartGetClockFreq(index);
#if defined(STM32CubeF3) || defined(STM32CubeL4)
    uint32_t baud_reg = UART_DIV_SAMPLING16(clk, baud);
#else
    uint32_t baud_reg = UART_DIV_SAMPLING16(clk, baud, pUart->hUart.Init.ClockPrescaler);
#endif
    __HAL_UART_DISABLE(%uUart->hUart);
    uart->hUart.Instance->BRR = baud_reg;
    __HAL_UART_ENABLE(&uart->hUart);

    return MHAL_STATUS_OK;
}

mHalStatus mHalUart_StateMachine(Uart uart) {
    mHalStatus retval = MHAL_STATUS_ERROR;
    if (!uart) {
        return MHAL_STATUS_ERROR_INSTANCE;
    }

    if (!IsUartEnabled(uart)) {
        return MHAL_STATUS_ERROR_DISABLED;
    }

    switch (uart->state_machine.state) {
    case UART_STATE_OK:
        if (uart->state_machine.loopback) {
            uint8_t data;
            if (HalUart_GetByte(uart->index, &data) == MHAL_STATUS_OK) {
                HalUart_SendData(uart->index, &data, 1);
            }
        }
        if (!UartReadyToSend(uart)) {
            retval = MHAL_STATUS_OK;
            break;
        } else {
            uint8_t byte;
            RingBuffer_GetData(uart->tx_buffer, &byte, 1);
            if (HAL_UART_Transmit_IT(&uart->hUart, &byte, 1) != HAL_OK) {
                retval = MHAL_STATUS_BUSY;
                uart->state_machine.pending_error = MHAL_STATUS_ERROR_IO;
                uart->state_machine.state = UART_STATE_ERROR;
                break;
            } else {
                uart->tx_busy = true;
                HalTimer_StartTimerObj(&uart->state_machine.timer);
            }
        }
        // fall-thru
    case UART_STATE_BUSY:
        if (HalTimer_TimerObjElapsed(&uart->state_machine.timer, UART_TX_TIMEOUT_USEC)) {
            HAL_UART_AbortTransmit_IT(&uart->hUart);
            uart->state_machine.pending_error = MHAL_STATUS_ERROR_TIMEOUT;
            uart->state_machine.state = UART_STATE_ERROR;
        }
        retval = MHAL_STATUS_BUSY;
        break;
    case UART_STATE_DONE:
        uart->tx_busy = false;
        uart->state_machine.state = UART_STATE_OK;
        retval = MHAL_STATUS_DONE;
        break;
    case UART_STATE_ERROR:
        retval = uart->state_machine.pending_error;
        uart->state_machine.error_count++;
        uart->state_machine.pending_error = MHAL_STATUS_OK;
        uart->state_machine.state = UART_STATE_OK;
        break;
    default:
        break;
    }

    return retval;
}

/*****************************************************************************/
/*                           INTERRUPT HANDLERS                              */
/*****************************************************************************/

/**
  * @brief Hardware interrupt handler for UART1
  */
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_inst[UART_INDEX_1].hUart);
}

/**
  * @brief Hardware interrupt handler for UART2
  */
void USART2_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_inst[UART_INDEX_2].hUart);
}

/**
  * @brief Hardware interrupt handler for UART3
  */
void USART3_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_inst[UART_INDEX_3].hUart);
}

/**
  * @brief Hardware interrupt handler for UART4
  */
void USART4_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_inst[UART_INDEX_4].hUart);
}

/**
  * @brief Hardware interrupt handler for UART5
  */
void USART5_IRQHandler(void) {
    HAL_UART_IRQHandler(&uart_inst[UART_INDEX_5].hUart);
}
