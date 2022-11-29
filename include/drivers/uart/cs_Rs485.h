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

#define RS485_BAUD_MIN 110
#define RS485_BAUD_MAX 115200

#define UART_BUFFER_SIZE 256
#define UART_BUFFER_QUEUE_SIZE 10

struct cs_uart_config {
	uint32_t baudrate;
	enum uart_config_parity parity;
	enum uart_config_stop_bits stop_bits;
};

class Rs485
{
public:
	static Rs485 &getInstance()
	{
		static Rs485 instance;
		return instance;
	}
	Rs485(Rs485 const &) = delete;
	void operator=(Rs485 const &) = delete;

	cs_err_t init(struct cs_uart_config *cfg);
	void sendUartMessage(uint8_t *msg, int len);
	uint8_t *getUartMessage();

private:
	Rs485() {}
	static void handleUartInterrupt(const struct device *dev, void *user_data);

	bool _isInitialized = false;
	const struct device *_rs485_dev = NULL;
	
	uint8_t _uart_rx_buf[UART_BUFFER_SIZE] = {0};
	uint16_t _uart_rx_buf_ctr = 0;
	
	uint8_t *_uart_tx_buf = NULL;
	uint8_t *_uart_tx_buf_ptr = NULL;
	uint16_t _uart_tx_buf_ctr = 0;
};