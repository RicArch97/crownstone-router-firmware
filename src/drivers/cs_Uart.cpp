/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#include <errno.h>
#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Uart, LOG_LEVEL_INF);

#include "cs_ReturnTypes.h"
#include "drivers/cs_Uart.h"

/**
 * @brief Initialize the UART module.
 *
 * @param cfg Optional struct with the mode, custom baudrate, parity and stop bits.
 * when NULL is provided, raw mode and 9600,8,n,1 is used.
 *
 * @return CS_OK if the UART module was sucessfully initialized.
 */
cs_err_t Uart::init(struct cs_uart_config *cfg)
{
	if (_isInitialized) {
		LOG_ERR("Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	// check if device is ready, and save instance
	if (!device_is_ready(_uart_dev)) {
		LOG_ERR("Uart device %s is not ready", _uart_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

	// configure uart parameters
	struct uart_config uart_cfg = {0};
	uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_cfg.data_bits = UART_CFG_DATA_BITS_8;

	if (cfg == NULL) {
		_uart_mode = CS_UART_MODE_RAW;

		uart_cfg.baudrate = CS_UART_RS_BAUD_DEFAULT;
		uart_cfg.parity = UART_CFG_PARITY_NONE;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
	} else {
		switch (cfg->mode) {
		case CS_UART_MODE_RAW:
			// according to RS485 and RS232 spec, baudrate between 110 and 115200
			uart_cfg.baudrate = CLAMP(cfg->serial_cfg.baudrate, CS_UART_RS_BAUD_MIN,
						  CS_UART_RS_BAUD_MAX);

			// use a total of 11 bits
			switch (cfg->serial_cfg.parity) {
			case UART_CFG_PARITY_ODD:
			case UART_CFG_PARITY_EVEN:
				uart_cfg.parity = cfg->serial_cfg.parity;
				uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
				break;
			case UART_CFG_PARITY_NONE:
				uart_cfg.parity = cfg->serial_cfg.parity;
				uart_cfg.stop_bits = cfg->serial_cfg.stop_bits;
				break;
			default:
				LOG_ERR("Invalid parity bit option provided");
				return CS_ERR_UART_CONFIG_INVALID;
			}
			break;
		case CS_UART_MODE_PACKETS:
			uart_cfg.baudrate = cfg->serial_cfg.baudrate;
			uart_cfg.parity = cfg->serial_cfg.parity;
			uart_cfg.stop_bits = cfg->serial_cfg.stop_bits;
			break;
		default:
			LOG_ERR("Invalid uart mode provided");
			return CS_ERR_UART_CONFIG_INVALID;
		}

		_uart_mode = cfg->mode;
	}

	if (uart_configure(_uart_dev, &uart_cfg) != 0) {
		LOG_ERR("Failed to configure uart");
		return CS_ERR_UART_CONFIG_FAILED;
	}

	// initialize message queue
	// aligned to 4-byte boundary
	char __aligned(4) msgq_buf[CS_UART_BUFFER_QUEUE_SIZE * CS_UART_BUFFER_SIZE];
	k_msgq_init(&_msgq_uart_msgs, msgq_buf, CS_UART_BUFFER_SIZE, CS_UART_BUFFER_QUEUE_SIZE);

	// set ISR, pass pointer to this class object as user data
	uart_irq_callback_user_data_set(_uart_dev, handleUartInterrupt, this);

	// start listening on RX
	uart_irq_rx_enable(_uart_dev);

	return CS_OK;
}

/**
 * @brief Get one UART message from the message queue.
 *
 * @return Pointer to buffer with the message allocated on the heap, NULL on empty queue or fail.
 */
uint8_t *Uart::getUartMessage()
{
	uint8_t *buf = (uint8_t *)calloc(CS_UART_BUFFER_SIZE, sizeof(uint8_t));
	if (buf == NULL) {
		LOG_ERR("Failed to allocate memory for uart message buffer");
		return NULL;
	}

	// wait till at least one message is received
	if (k_msgq_get(&_msgq_uart_msgs, &buf, K_FOREVER) == 0) {
		return buf;
	}

	return NULL;
}

/**
 * @brief Transmit a message over UART.
 *
 * @param msg Pointer to the buffer with the message data.
 * @param len Length of the message data.
 */
void Uart::sendUartMessage(uint8_t *msg, int len)
{
	if (_uart_tx_buf != NULL) {
		k_free(_uart_tx_buf);
	}
	_uart_tx_buf = (uint8_t *)k_calloc(len, sizeof(uint8_t));

	memcpy(_uart_tx_buf, msg, len);
	_uart_tx_buf_ctr = len;
	_uart_tx_buf_ptr = &_uart_tx_buf[0];

	uart_irq_rx_disable(_uart_dev);
	uart_irq_tx_enable(_uart_dev);
}

/**
 * @brief Disable all UART interrupts.
 */
void Uart::disable()
{
	uart_irq_rx_disable(_uart_dev);
	uart_irq_tx_disable(_uart_dev);
}

/**
 * @brief Handle UART data.
 * Interrupt on RX is handled byte by byte, complete buffers sent to message queue
 */
void Uart::handleUartInterrupt(const struct device *dev, void *user_data)
{
	if (!uart_irq_update(dev) && !uart_irq_is_pending(dev)) {
		return;
	}

	// class object pointer was passed at initialization
	Uart *uart_inst = (Uart *)user_data;

	// handle interrupt on RX
	if (uart_irq_rx_ready(dev)) {
		uint8_t c;
		// read one byte from the uart fifo queue
		if (uart_fifo_read(dev, &c, 1) != 1) {
			LOG_ERR("Failed to read from uart fifo");
			return;
		}

		// store characters until line end is detected, or buffer if full
		if (((c == '\n' || c == '\r') && (uart_inst->_uart_rx_buf_ctr > 0)) ||
		    (uart_inst->_uart_rx_buf_ctr == (CS_UART_BUFFER_SIZE - 1))) {
			// terminate string
			uart_inst->_uart_rx_buf[uart_inst->_uart_rx_buf_ctr] = '\0';

			// add to message queue, this copies the data to ring buffer
			k_msgq_put(&uart_inst->_msgq_uart_msgs, &uart_inst->_uart_rx_buf,
				   K_NO_WAIT);

			uart_inst->_uart_rx_buf_ctr = 0;
			memset(uart_inst->_uart_rx_buf, 0, sizeof(uart_inst->_uart_rx_buf));
		} else if (uart_inst->_uart_rx_buf_ctr < (CS_UART_BUFFER_SIZE - 1)) {
			uart_inst->_uart_rx_buf[uart_inst->_uart_rx_buf_ctr++] = c;
		}
		// else: unhandled, pass
	}

	// handle interrupt on TX
	if (uart_irq_tx_ready(dev)) {
		if (uart_inst->_uart_tx_buf_ctr > 0) {
			int n = uart_fifo_fill(dev, uart_inst->_uart_tx_buf_ptr,
					       uart_inst->_uart_tx_buf_ctr);
			uart_inst->_uart_tx_buf_ctr -= n;
			uart_inst->_uart_tx_buf_ptr += n;
		}

		// check if all bytes were transmitted to avoid corrupted message
		if (uart_irq_tx_complete(dev)) {
			k_free(uart_inst->_uart_tx_buf);
			uart_irq_tx_disable(dev);
			uart_irq_rx_enable(dev);
		}
	}
}

/**
 * @brief Free all heap allocated memory.
 */
Uart::~Uart()
{
	k_free(_uart_tx_buf);
}