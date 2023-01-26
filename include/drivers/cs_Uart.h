/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"
#include "cs_RouterProtocol.h"
#include "cs_PacketHandling.h"

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
#define CS_UART_BUFFER_QUEUE_SIZE 3

#define CS_UART_THREAD_PRIORITY	  K_PRIO_PREEMPT(7)
#define CS_UART_THREAD_STACK_SIZE 4096

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
	 * @brief Uart constructor for data packaging and transporting.
	 *
	 * @param dev Pointer to UART device structure
	 * @param src_id Identifier for the UART device, used in UART packets
	 * @param dest_id Destination identifier for where the data should be transported to
	 * @param handler PacketHandler instance
	 */
	Uart(const device *dev, cs_router_instance_id src_id, cs_router_instance_id dest_id,
	     PacketHandler *handler)
		: _uart_dev(dev), _dest_id(dest_id), _src_id(src_id), _pkt_handler(handler){};
	~Uart();

	cs_ret_code_t init(cs_uart_config *cfg);
	void disable();

	static void sendUartMessage(void *inst, uint8_t *msg, int msg_len);

	/** Initialized flag */
	bool _initialized = false;

	/** UART device structure, holding information about the current UART hardware */
	const device *_uart_dev = NULL;

	/** Destination id, where UART packets from this instance should be routed to */
	cs_router_instance_id _dest_id = CS_INSTANCE_ID_UNKNOWN;
	/** UART source id, identifying the UART device from which data is sent */
	cs_router_instance_id _src_id = CS_INSTANCE_ID_UNKNOWN;
	/** PacketHanler instance to handle messages and packets */
	PacketHandler *_pkt_handler = NULL;

	/** UART message queue structure instance */
	k_msgq _uart_msgq;
	/** UART message buffer used by the message queue */
	char __aligned(4) _msgq_buf[CS_UART_BUFFER_QUEUE_SIZE * CS_UART_BUFFER_SIZE];

	/** UART thread structure instance */
	k_thread _uart_tid;

	/** UART buffer of 256 bytes */
	uint8_t _uart_buf[CS_UART_BUFFER_SIZE];
	/** Counter for the amount of bytes currently in the UART buffer */
	uint16_t _uart_buf_ctr = 0;
	/** Pointer that points to the last handled byte in the UART buffer */
	uint8_t *_uart_buf_ptr = NULL;
};