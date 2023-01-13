/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "socket/cs_WebSocket.h"
#include "cs_ReturnTypes.h"
#include "cs_Router.h"

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/time_units.h>

#include <stdint.h>

// supported baudrates by RS232 and RS485
#define CS_UART_RS_BAUD_MIN	110
#define CS_UART_RS_BAUD_MAX	115200
#define CS_UART_RS_BAUD_DEFAULT 9600

#define CS_UART_BUFFER_SIZE	  256
#define CS_UART_BUFFER_QUEUE_SIZE 10

#define CS_UART_THREAD_PRIORITY	  K_PRIO_PREEMPT(5)
#define CS_UART_THREAD_STACK_SIZE 4096
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
	Uart() = default;
	/**
	 * @brief Uart constructor.
	 *
	 * @param dev Pointer to UART device structure.
	 * @param src_id Identifier for the UART device, used in UART packets.
	 */
	Uart(const device *dev, cs_router_instance_uart_id src_id)
		: _uart_dev(dev), _src_id(src_id){};
	~Uart();

	cs_err_t init(cs_uart_config *cfg);
	void sendUartMessage(uint8_t *msg, size_t len);
	int wrapUartMessage(uint8_t *message, uint8_t *pkt_buf);
	void disable();

	/** Destination type, where UART packets from this instance should be routed to */
	cs_router_instance_type _dest_type;
	/** Instances where UART packets can be routed to */
	cs_router_instances *_inst = NULL;

	/** UART message queue structure instance */
	k_msgq _msgq_uart_msgs;
	/** UART message buffer used by the message queue */
	char __aligned(4) _msgq_buf[CS_UART_BUFFER_QUEUE_SIZE * CS_UART_BUFFER_SIZE];

	/** UART thread structure instance */
 	k_thread _uart_tid;

	/** UART RX buffer of 256 bytes containing raw data received over UART */
	uint8_t _uart_rx_buf[CS_UART_BUFFER_SIZE];
	/** Counter for the amount of bytes currently in the RX buffer */
	uint16_t _uart_rx_buf_ctr = 0;

	/** Heap allocated UART TX buffer, size depends on the message */
	uint8_t *_uart_tx_buf = NULL;
	/** Pointer that points to the last handled byte in the TX buffer */
	uint8_t *_uart_tx_buf_ptr = NULL;
	/** Counter for the amount of bytes currently in the TX buffer */
	uint16_t _uart_tx_buf_ctr = 0;

private:
	/** Initialized flag */
	bool _initialized = false;

	/** UART device structure, holding information about the current UART hardware */
	const device *_uart_dev = NULL;

	/** UART source id, identifying the UART device from which data is sent */
	cs_router_instance_uart_id _src_id;
};