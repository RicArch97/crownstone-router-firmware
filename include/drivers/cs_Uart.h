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

// supported baudrates by RS232 and RS485
#define CS_UART_BAUD_MIN     110
#define CS_UART_BAUD_MAX     115200
#define CS_UART_BAUD_DEFAULT 9600

#define CS_UART_BUFFER_SIZE	  256
#define CS_UART_BUFFER_QUEUE_SIZE 10

struct cs_uart_config {
	uint32_t baudrate;
	enum uart_config_parity parity;
	enum uart_config_stop_bits stop_bits;
};

class Uart
{
public:
	Uart() {}
	Uart(const struct device *dev)
	{
		_uart_dev = dev;
	}
	~Uart();

	cs_err_t init(struct cs_uart_config *cfg);
	void sendUartMessage(uint8_t *msg, int len);
	uint8_t *getUartMessage();
	void disable();

private:
	static void handleUartInterrupt(const struct device *dev, void *user_data);

	bool _isInitialized = false;
	const struct device *_uart_dev = NULL;
	struct k_msgq _msgq_uart_msgs;

	uint8_t _uart_rx_buf[CS_UART_BUFFER_SIZE] = {0};
	uint16_t _uart_rx_buf_ctr = 0;

	uint8_t *_uart_tx_buf = NULL;
	uint8_t *_uart_tx_buf_ptr = NULL;
	uint16_t _uart_tx_buf_ctr = 0;
};