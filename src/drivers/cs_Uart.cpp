/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Uart.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Uart, LOG_LEVEL_INF);

#include <string.h>

K_THREAD_STACK_DEFINE(uart_tid_stack_area, CS_UART_THREAD_STACK_SIZE);

/**
 * @brief Thread function that handles messages in the UART message queue.
 *
 * @param inst Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
static void handleUartMessages(void *inst, void *unused1, void *unused2)
{
	Uart *uart_inst = static_cast<Uart *>(inst);
	uint8_t msg_buf[CS_UART_BUFFER_SIZE];

	while (1) {
		// wait till message is retrieved from message queue
		if (k_msgq_get(&uart_inst->_uart_msgq, &msg_buf, K_FOREVER) == 0) {
			LOG_HEXDUMP_DBG(msg_buf, (uint32_t)strlen((char *)msg_buf), "uart message");

			cs_packet_data uart_data;
			memset(&uart_data, 0, sizeof(uart_data));
			// UART buffers are null terminated, so get length with strlen
			size_t msg_len = MIN(CS_PACKET_BUF_SIZE, strlen((char *)msg_buf));
			uart_data.msg.buf_len = msg_len;
			uart_data.dest_id = uart_inst->_dest_id;
			memcpy(&uart_data.msg.buf, msg_buf, msg_len);

			// packets sent from CM4 start with a specific token
			// handle the packet as incoming
			if (msg_buf[0] == CS_PACKET_UART_START_TOKEN) {
				uart_data.src_id = CS_INSTANCE_ID_UART_CM4;
				uart_data.type = CS_DATA_INCOMING;
			} else {
				// packet is sent from this instance, use own source id
				uart_data.src_id = uart_inst->_src_id;
				uart_data.type = CS_DATA_OUTGOING;
			}

			if (uart_inst->_pkt_handler != NULL) {
				// dispatch the work item
				int ret = uart_inst->_pkt_handler->handlePacket(&uart_data);
				// handler not running due to error, abort
				if (ret == CS_ERR_ABORTED) {
					break;
				}
			} else {
				LOG_WRN("%s", "Failed to handle UART message");
				break;
			}
		}
	}
}

/**
 * @brief Handle UART interrupts.
 * Interrupt on RX is handled byte by byte, complete buffers sent to message queue.
 */
static void handleUartInterrupt(const device *dev, void *user_data)
{
	if (!uart_irq_update(dev) && !uart_irq_is_pending(dev)) {
		return;
	}

	// class object pointer was passed at initialization
	Uart *uart_inst = static_cast<Uart *>(user_data);

	// handle interrupt on RX
	if (uart_irq_rx_ready(dev)) {
		uint8_t c;
		// read one byte from the uart fifo queue
		if (uart_fifo_read(dev, &c, 1) != 1) {
			LOG_ERR("%s", "Failed to read from uart fifo");
			return;
		}

		// store characters until line end is detected, or buffer if full
		if (c == '\n' || c == '\r' ||
		    uart_inst->_uart_buf_ctr == (CS_UART_BUFFER_SIZE - 1)) {
			if (uart_inst->_uart_buf_ctr > 0) {
				// terminate string
				uart_inst->_uart_buf[uart_inst->_uart_buf_ctr] = '\0';

				// add to message queue, this copies the data to ring buffer
				// if queue is full, purge old data and try again
				if (k_msgq_put(&uart_inst->_uart_msgq, &uart_inst->_uart_buf,
					       K_NO_WAIT) != 0) {
					k_msgq_purge(&uart_inst->_uart_msgq);
				}
				uart_inst->_uart_buf_ctr = 0;
			}
		} else if (uart_inst->_uart_buf_ctr < (CS_UART_BUFFER_SIZE - 1)) {
			uart_inst->_uart_buf[uart_inst->_uart_buf_ctr++] = c;
		}
	}

	// handle interrupt on TX
	if (uart_irq_tx_ready(dev)) {
		if (uart_inst->_uart_buf_ctr > 0) {
			int n = uart_fifo_fill(dev, uart_inst->_uart_buf_ptr,
					       uart_inst->_uart_buf_ctr);
			uart_inst->_uart_buf_ctr -= n;
			uart_inst->_uart_buf_ptr += n;
		}

		// check if all bytes were transmitted to avoid corrupted message
		if (uart_irq_tx_complete(dev)) {
			uart_irq_tx_disable(dev);
			uart_irq_rx_enable(dev);
		}
	}
}

/**
 * @brief Initialize the UART module.
 *
 * @param cfg Optional struct with custom baudrate, parity and stop bits.
 * when NULL is provided, 9600,8,n,1 is used.
 *
 * @return CS_OK if the UART module was sucessfully initialized.
 */
cs_ret_code_t Uart::init(cs_uart_config *cfg)
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	// check if device is ready, and save instance
	if (!device_is_ready(_uart_dev)) {
		LOG_ERR("Uart device %s is not ready", _uart_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

	// configure uart parameters
	uart_config uart_cfg = {0};
	uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_cfg.data_bits = UART_CFG_DATA_BITS_8;

	if (cfg == NULL) {
		uart_cfg.baudrate = CS_UART_RS_BAUD_DEFAULT;
		uart_cfg.parity = UART_CFG_PARITY_NONE;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
	} else {
		if (_src_id == CS_INSTANCE_ID_UART_CM4) {
			// CM4 UART connection is not using any RS protocol, so not constrained
			uart_cfg.baudrate = cfg->baudrate;
			uart_cfg.parity = cfg->parity;
			uart_cfg.stop_bits = cfg->stop_bits;
		} else {
			// according to RS485 and RS232 spec, baudrate between 110 and 115200
			uart_cfg.baudrate =
				CLAMP(cfg->baudrate, CS_UART_RS_BAUD_MIN, CS_UART_RS_BAUD_MAX);

			// use a total of 11 bits
			switch (cfg->parity) {
			case UART_CFG_PARITY_ODD:
			case UART_CFG_PARITY_EVEN:
				uart_cfg.parity = cfg->parity;
				uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
				break;
			case UART_CFG_PARITY_NONE:
				uart_cfg.parity = cfg->parity;
				uart_cfg.stop_bits = cfg->stop_bits;
				break;
			default:
				LOG_ERR("%s", "Invalid parity bit option provided");
				return CS_ERR_UART_CONFIG_INVALID;
			}
		}
	}

	if (uart_configure(_uart_dev, &uart_cfg) != 0) {
		LOG_ERR("%s", "Failed to configure uart");
		return CS_ERR_UART_CONFIG_FAILED;
	}

	// initialize message queue, aligned to 4-byte boundary
	k_msgq_init(&_uart_msgq, _msgq_buf, CS_UART_BUFFER_SIZE, CS_UART_BUFFER_QUEUE_SIZE);

	// set ISR, pass pointer to this class object as user data
	uart_irq_callback_user_data_set(_uart_dev, handleUartInterrupt, this);

	// start listening on RX
	uart_irq_rx_enable(_uart_dev);

	// create thread for handling uart messages
	k_tid_t uart_thread = k_thread_create(
		&_uart_tid, uart_tid_stack_area, K_THREAD_STACK_SIZEOF(uart_tid_stack_area),
		handleUartMessages, this, NULL, NULL, CS_UART_THREAD_PRIORITY, 0, K_NO_WAIT);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Transmit a message over UART. Callback function for PacketHandler.
 *
 * @param inst Pointer to UART class instance.
 * @param message Pointer to buffer with the message.
 * @param len Length of the message.
 */
void Uart::sendUartMessage(k_work *work)
{
	cs_packet_handler *hdlr = CONTAINER_OF(work, cs_packet_handler, work_item);
	Uart *uart_inst = static_cast<Uart *>(hdlr->target_inst);
	k_spinlock_key_t key;

	if (!uart_inst->_initialized) {
		LOG_ERR("%s", "Not initialized");
		return;
	}

	key = k_spin_lock(&hdlr->work_lock);

	memcpy(uart_inst->_uart_buf, hdlr->msg.buf, hdlr->msg.buf_len);
	uart_inst->_uart_buf_ctr = hdlr->msg.buf_len;
	uart_inst->_uart_buf_ptr = &uart_inst->_uart_buf[0];

	k_spin_unlock(&hdlr->work_lock, key);

	uart_irq_rx_disable(uart_inst->_uart_dev);
	uart_irq_tx_enable(uart_inst->_uart_dev);
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
 * @brief Free all allocated memory.
 */
Uart::~Uart()
{
	disable();
	k_msgq_cleanup(&_uart_msgq);
}