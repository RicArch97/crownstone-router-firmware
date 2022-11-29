/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#include <errno.h>
#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Rs485, LOG_LEVEL_INF);

#include "cs_ReturnTypes.h"
#include "drivers/uart/cs_Rs485.h"

#define RS485_UART_MODULE DT_NODELABEL(uart0)

static struct k_msgq msgq_uart_msgs;

/**
 * @brief Initialize the RS485 module.
 *
 * @return CS_OK if the RS485 module was sucessfully initialized.
 */
cs_err_t Rs485::init(struct cs_uart_config *cfg)
{
	if (_isInitialized) {
		LOG_ERR("Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	_rs485_dev = DEVICE_DT_GET(RS485_UART_MODULE);
	// obtain a device reference and check if device is ready
	if (!device_is_ready(_rs485_dev)) {
		LOG_ERR("RS485 device %s is not ready", _rs485_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

	// configure uart parameters
	struct uart_config uart_cfg = {0};
	uart_cfg.baudrate = CLAMP(cfg->baudrate, RS485_BAUD_MIN, RS485_BAUD_MAX);
	uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_cfg.data_bits = UART_CFG_DATA_BITS_8;

	switch (cfg->parity) {
	case UART_CFG_PARITY_ODD:
	case UART_CFG_PARITY_EVEN:
		uart_cfg.parity = cfg->parity;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
		break;
	case UART_CFG_PARITY_NONE:
		// requires 2 stop bits
		uart_cfg.parity = cfg->parity;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_2;
		break;
	default:
		return CS_ERR_UART_CONFIG_INVALID;
	}

	if (uart_configure(_rs485_dev, &uart_cfg) != 0) {
		LOG_ERR("Failed to configure uart");
		return CS_ERR_UART_CONFIG_FAILED;
	}

	// initialize message queue
	// aligned to 4-byte boundary
	char __aligned(4) msgq_buf[UART_BUFFER_QUEUE_SIZE * UART_BUFFER_SIZE];
	k_msgq_init(&msgq_uart_msgs, msgq_buf, UART_BUFFER_SIZE, UART_BUFFER_QUEUE_SIZE);

	// set ISR
	uart_irq_callback_user_data_set(_rs485_dev, handleUartInterrupt, NULL);

	// start listening on RX
	uart_irq_rx_enable(_rs485_dev);

	return CS_OK;
}

/**
 * @brief Get one UART message from the message queue.
 *
 * @return Pointer to buffer with the message allocated on the heap, NULL on empty queue or fail.
 */
uint8_t *Rs485::getUartMessage()
{
	uint8_t *buf = (uint8_t*)calloc(UART_BUFFER_SIZE, sizeof(uint8_t));
	if (buf == NULL) {
		LOG_ERR("Failed to allocate memory for uart message buffer");
		return NULL;
	}

	// wait till at least one message is received
	if (k_msgq_get(&msgq_uart_msgs, &buf, K_FOREVER) == 0) {
		return buf;
	}

	return NULL;
}

/**
 * @brief Transmit a message over UART.
 */
void Rs485::sendUartMessage(uint8_t *msg, int len)
{
	if (_uart_tx_buf != NULL) {
		k_free(_uart_tx_buf);
	}
	_uart_tx_buf = (uint8_t *)k_calloc(len, sizeof(uint8_t));

	memcpy(_uart_tx_buf, msg, len);
	_uart_tx_buf_ctr = len;
	_uart_tx_buf_ptr = &_uart_tx_buf[0];

	uart_irq_rx_disable(_rs485_dev);
	uart_irq_tx_enable(_rs485_dev);
}

/**
 * @brief Handle UART data.
 * Interrupt on RX is handled byte by byte, complete buffers sent to message queue
 */
void Rs485::handleUartInterrupt(const struct device *dev, void *user_data)
{
	if (!uart_irq_update(dev) && !uart_irq_is_pending(dev)) {
		return;
	}

	Rs485 *rs485 = &Rs485::getInstance();

	// handle interrupt on RX
	if (uart_irq_rx_ready(dev)) {
		uint8_t c;
		// read one byte from the uart fifo queue
		if (uart_fifo_read(dev, &c, 1) != 1) {
			LOG_ERR("Failed to read from uart fifo");
			return;
		}

		// store characters until line end is detected, or buffer if full
		if (((c == '\n' || c == '\r') && (rs485->_uart_rx_buf_ctr > 0)) ||
		    (rs485->_uart_rx_buf_ctr == (UART_BUFFER_SIZE - 1))) {
			// terminate string
			rs485->_uart_rx_buf[rs485->_uart_rx_buf_ctr] = '\0';

			// add to message queue, this copies the data to ring buffer
			k_msgq_put(&msgq_uart_msgs, &rs485->_uart_rx_buf, K_NO_WAIT);

			rs485->_uart_rx_buf_ctr = 0;
			memset(rs485->_uart_rx_buf, 0, sizeof(rs485->_uart_rx_buf));
		} else if (rs485->_uart_rx_buf_ctr < (UART_BUFFER_SIZE - 1)) {
			rs485->_uart_rx_buf[rs485->_uart_rx_buf_ctr++] = c;
		}
		// else: unhandled, do nothing
	}

	// handle interrupt on TX
	if (uart_irq_tx_ready(dev)) {
		if (rs485->_uart_tx_buf_ctr > 0) {
			int n = uart_fifo_fill(dev, rs485->_uart_tx_buf_ptr,
					       rs485->_uart_tx_buf_ctr);
			rs485->_uart_tx_buf_ctr -= n;
			rs485->_uart_tx_buf_ptr += n;
		}

		// check if all bytes were transmitted to avoid corrupted message
		if (uart_irq_tx_complete(dev)) {
			k_free(rs485->_uart_tx_buf);
			uart_irq_tx_disable(dev);
			uart_irq_rx_enable(dev);
		}
	}
}