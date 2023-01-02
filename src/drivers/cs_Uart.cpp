/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Uart, LOG_LEVEL_INF);

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

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
	if (_is_initialized) {
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

	_is_initialized = true;

	return CS_OK;
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
 * @brief Thread function that handles messages in the UART message queue.
 *
 * @param cls Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
void Uart::uartManager(void *cls, void *unused1, void *unused2)
{
	Uart *uart_inst = (Uart *)cls;

	if (!uart_inst->_is_initialized) {
		LOG_ERR("Not initialized");
		return;
	}

	while (1) {
		// check if there are messages in the queue
		if (k_msgq_num_used_get(&uart_inst->_msgq_uart_msgs) == 0) {
			continue;
		}

		uint8_t *buf = (uint8_t *)k_malloc(CS_UART_BUFFER_SIZE * sizeof(uint8_t));
		if (buf == NULL) {
			LOG_ERR("Failed to allocate memory for uart message buffer");
			break;
		}

		// poll the message queue (no wait)
		if (k_msgq_get(&uart_inst->_msgq_uart_msgs, &buf, K_NO_WAIT) != 0) {
			// packet sent from CM4 start with a specific token
			if (buf[0] == CS_UART_CM4_START_TOKEN) {
				uart_inst->handleUartPacket(buf);
			} else {
				uint8_t *packet = uart_inst->wrapUartMessage(buf);

				// TODO: mailbox for synchronization with websocket thread?
			}
		}

		k_sleep(K_MSEC(200));
	}
}

/**
 * @brief Wrap raw UART data into a packet.
 *
 * @param message Pointer to UART message buffer.
 *
 * @return Heap allocated pointer to packet buffer if successful, else NULL.
 */
uint8_t *Uart::wrapUartMessage(uint8_t *message)
{
	LOG_DBG("Wrapping raw data %s into packet", message);

	// buffer is string terminated, might be shorter than 256 on newline
	int payload_len = strlen((char *)message);
	int data_pkt_len = (sizeof(cs_router_data_packet) - sizeof(uint8_t *)) + payload_len;
	uint8_t *data_pkt = (uint8_t *)k_malloc(data_pkt_len * sizeof(uint8_t));
	if (data_pkt == NULL) {
		LOG_ERR("Failed to allocate memory for data packet");
		k_free(message);
		return NULL;
	}

	data_pkt[0] = CS_SOURCE_TYPE_UART;
	data_pkt[1] = _src_id;
	sys_put_le16(payload_len, data_pkt + 2);
	memcpy(data_pkt + 4, message, payload_len);
	k_free(message);

	int generic_pkt_len = (sizeof(cs_router_generic_packet) - sizeof(uint8_t *)) + data_pkt_len;
	uint8_t *generic_pkt = (uint8_t *)k_malloc(generic_pkt_len * sizeof(uint8_t));
	if (generic_pkt == NULL) {
		LOG_ERR("Failed to allocate memory for generic packet");
		k_free(data_pkt);
		return NULL;
	}

	generic_pkt[0] = CS_PROTOCOL_VERSION;
	generic_pkt[1] = CS_PACKET_TYPE_DATA;
	sys_put_le16(data_pkt_len, generic_pkt + 2);
	memcpy(generic_pkt + 4, data_pkt, data_pkt_len);
	k_free(data_pkt);

	// if data should be send to the CM4 over UART, wrap into UART packet
	if (_src_id == CS_SOURCE_ID_UART_CM4) {
		int uart_pkt_len =
			(sizeof(cs_router_uart_packet) - sizeof(uint8_t *)) + generic_pkt_len;
		uint8_t *uart_pkt = (uint8_t *)k_malloc(uart_pkt_len * sizeof(uint8_t));
		if (uart_pkt == NULL) {
			LOG_ERR("Failed to allocate memory for uart packet");
			k_free(generic_pkt);
			return NULL;
		}

		int uart_pkt_ctr = 0;

		uart_pkt[uart_pkt_ctr++] = CS_UART_CM4_START_TOKEN;
		sys_put_le16(uart_pkt_len - 3, uart_pkt + uart_pkt_ctr);
		uart_pkt_ctr += 2;
		uart_pkt[uart_pkt_ctr++] = CS_UART_PROTOCOL_VERSION;
		uart_pkt[uart_pkt_ctr++] = CS_PACKET_TYPE_GENERIC;
		memcpy(uart_pkt + uart_pkt_ctr, generic_pkt, generic_pkt_len);
		uart_pkt_ctr += generic_pkt_len;
		k_free(generic_pkt);

		// calculate CRC16 CCITT over everything after length (so
		// don't include start token and length)
		uint16_t crc = crc16_ccitt(CS_UART_CM4_CRC_SEED, uart_pkt + 3, uart_pkt_ctr - 3);
		sys_put_le16(crc, uart_pkt + uart_pkt_ctr);

		return uart_pkt;
	}

	return generic_pkt;
}

/**
 * @brief Handle UART packet received from CM4.
 *
 * @param Packet Pointer to the packet buffer.
 *
 * @return CS_OK if the packet was handled successfully, else CS_FAIL.
 */
void Uart::handleUartPacket(uint8_t *packet)
{
	LOG_DBG("Received UART packet from CM4 %s", packet);

	int uart_pkt_cm4_ctr = 0;

	struct cs_router_uart_packet uart_pkt_cm4;
	uart_pkt_cm4.start_token = packet[uart_pkt_cm4_ctr++];
	uart_pkt_cm4.length = sys_get_le16(packet + uart_pkt_cm4_ctr);
	uart_pkt_cm4_ctr += 2;
	uart_pkt_cm4.protocol_version = packet[uart_pkt_cm4_ctr++];
	uart_pkt_cm4.type = packet[uart_pkt_cm4_ctr++];

	int cm4_payload_len = uart_pkt_cm4.length - 4; // minus protocol ver, type and CRC
	uart_pkt_cm4.payload = (uint8_t *)k_malloc(cm4_payload_len * sizeof(uint8_t));
	if (uart_pkt_cm4.payload == NULL) {
		LOG_ERR("Failed to allocate memory for CM4 packet payload");
		k_free(packet);
		return;
	}
	memcpy(uart_pkt_cm4.payload, packet + uart_pkt_cm4_ctr, cm4_payload_len);
	uart_pkt_cm4_ctr += cm4_payload_len;

	// check CRC CCITT over everything after length, not including CRC (uint16) itself
	uint16_t check_crc = crc16_ccitt(CS_UART_CM4_CRC_SEED, packet + 3, uart_pkt_cm4.length - 2);
	uint16_t received_crc = sys_get_le16(packet + uart_pkt_cm4_ctr);
	// skip packet if CRC doesn't match
	if (check_crc != received_crc) {
		LOG_WRN("CRC mismatch on received CM4 packet. Calculated: %hu, Received: %hu",
			check_crc, received_crc);
		return;
	}

	// TODO: do something with packet
}

/**
 * @brief Free all heap allocated memory.
 */
Uart::~Uart()
{
	k_free(_uart_tx_buf);
	k_msgq_cleanup(&_msgq_uart_msgs);
}