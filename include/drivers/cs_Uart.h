/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/time_units.h>

#include "cs_ReturnTypes.h"
#include "cs_Router.h"
#include "socket/cs_WebSocket.h"

// supported baudrates by RS232 and RS485
#define CS_UART_RS_BAUD_MIN	110
#define CS_UART_RS_BAUD_MAX	115200
#define CS_UART_RS_BAUD_DEFAULT 9600

// signals start of a message
#define CS_UART_CM4_START_TOKEN 0x7E
// crc start value
#define CS_UART_CM4_CRC_SEED	0xFFFF

#define CS_UART_BUFFER_SIZE	  256
#define CS_UART_BUFFER_QUEUE_SIZE 10

#define CS_UART_THREAD_PRIORITY	  K_PRIO_PREEMPT(8)
#define CS_UART_THREAD_STACK_SIZE 1024
#define CS_UART_THREAD_SLEEP	  200

#define CS_UART_PACKET_BUF_SIZE 512

/**
 * @brief UART serial parameters, that both ends should agree on.
 *
 * @param baudrate Baudrate setting in bps
 * @param parity Parity bit, using one of @ref uart_config_parity
 * @param stop_bits Stop bits, using one of @ref uart_config_stop_bits
 */
struct cs_uart_config {
	uint32_t baudrate;
	enum uart_config_parity parity;
	enum uart_config_stop_bits stop_bits;
};

class Uart
{
public:
	Uart();
	/**
	 * @brief Uart constructor for initialization without target device.
	 *
	 * @param dev Pointer to UART device structure.
	 * @param src_id Identifier for the UART device, used in UART packets.
	 */
	Uart(const struct device *dev, enum cs_router_source_uart_id src_id)
	{
		_uart_dev = dev;
		_src_id = src_id;
	}
	/**
	 * @brief Uart constructor for initialization with websocket as target for packets.
	 *
	 * @param dev Pointer to UART device structure.
	 * @param src_id Identifier for the UART device, used in data packets.
	 * @param ws_inst Pointer to WebSocket instance that packets should be send to.
	 */
	Uart(const struct device *dev, enum cs_router_source_uart_id src_id, WebSocket *ws_inst)
	{
		_uart_dev = dev;
		_src_id = src_id;
		_ws_inst = ws_inst;
	}
	/**
	 * @brief Uart constructor for initialization with a UART connection as target for packets.
	 *
	 * @param dev Pointer to UART device structure.
	 * @param src_id Identifier for the UART device, used in data packets.
	 * @param uart_inst Pointer to Uart instance that packets should be send to.
	 */
	Uart(const struct device *dev, enum cs_router_source_uart_id src_id, Uart *uart_inst)
	{
		_uart_dev = dev;
		_src_id = src_id;
		_uart_inst = uart_inst;
	}
	~Uart();

	cs_err_t init(struct cs_uart_config *cfg);
	void sendUartMessage(uint8_t *msg, size_t len);
	void disable();

private:
	static void handleUartInterrupt(const struct device *dev, void *user_data);
	static void handleUartMessages(void *cls, void *unused1, void *unused2);
	int wrapUartMessage(uint8_t *message, uint8_t *pkt_buf);
	void handleUartPacket(uint8_t *packet);

	bool _is_initialized = false;
	const struct device *_uart_dev = NULL;
	enum cs_router_source_uart_id _src_id;

	WebSocket *_ws_inst = NULL;
	Uart *_uart_inst = NULL;

	struct k_msgq _msgq_uart_msgs;

	uint8_t _uart_rx_buf[CS_UART_BUFFER_SIZE] = {0};
	uint16_t _uart_rx_buf_ctr = 0;

	uint8_t *_uart_tx_buf = NULL;
	uint8_t *_uart_tx_buf_ptr = NULL;
	uint16_t _uart_tx_buf_ctr = 0;
};