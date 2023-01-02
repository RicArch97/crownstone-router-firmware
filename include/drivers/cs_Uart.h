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

// supported baudrates by RS232 and RS485
#define CS_UART_RS_BAUD_MIN	110
#define CS_UART_RS_BAUD_MAX	115200
#define CS_UART_RS_BAUD_DEFAULT 9600

// signals start of a message
#define CS_UART_CM4_START_TOKEN 0x7E
// crc start value
#define CS_UART_CM4_CRC_SEED 0xFFFF

#define CS_UART_BUFFER_SIZE	  256
#define CS_UART_BUFFER_QUEUE_SIZE 10

/**
 * @brief The expected data in the UART connection.
 * For RS interfaces it is unknown which data is sent, which is the RAW mode.
 * The PACKETS mode is a packet according to the router communication protocol.
 */
enum cs_uart_mode {
	CS_UART_MODE_RAW,
	CS_UART_MODE_PACKETS
};

/**
 * @brief UART serial parameters, that both ends should agree on.
 *
 * @param baudrate Baudrate setting in bps
 * @param parity Parity bit, using one of @ref uart_config_parity
 * @param stop_bits Stop bits, using one of @ref uart_config_stop_bits
 */
struct cs_uart_serial_params {
	uint32_t baudrate;
	enum uart_config_parity parity;
	enum uart_config_stop_bits stop_bits;
};

/**
 * @brief Specific UART configuration, depending on the connection.
 *
 * @param mode Which UART mode applies to this connection, one of @ref cs_uart_mode
 * @param serial_cfg Serial parameters to use for the connection
 */
struct cs_uart_config {
	enum cs_uart_mode mode;
	struct cs_uart_serial_params serial_cfg;
};

class Uart
{
public:
	Uart();
	Uart(const struct device *dev, enum cs_router_source_uart_id src_id)
	{
		_uart_dev = dev;
		_src_id = src_id;
	}
	~Uart();

	cs_err_t init(struct cs_uart_config *cfg);
	void sendUartMessage(uint8_t *msg, int len);
	void disable();

private:
	static void handleUartInterrupt(const struct device *dev, void *user_data);
	static void uartManager(void *cls, void *unused1, void *unused2);
	uint8_t *wrapUartMessage(uint8_t *message);
	void handleUartPacket(uint8_t *packet);

	bool _is_initialized = false;
	const struct device *_uart_dev = NULL;
	enum cs_router_source_uart_id _src_id;
	enum cs_uart_mode _uart_mode;
	struct k_msgq _msgq_uart_msgs;

	uint8_t _uart_rx_buf[CS_UART_BUFFER_SIZE] = {0};
	uint16_t _uart_rx_buf_ctr = 0;

	uint8_t *_uart_tx_buf = NULL;
	uint8_t *_uart_tx_buf_ptr = NULL;
	uint16_t _uart_tx_buf_ctr = 0;
};