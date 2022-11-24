/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/sys/time_units.h>

#include "cs_ReturnTypes.h"

#define RS485_BAUD_MIN 110
#define RS485_BAUD_MAX 115200

#define NUM_BITS	 11
#define UART_BUFFER_SIZE 256
#define MAX_DELAY	 3500000

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
	RS485(Rs485 const &) = delete;
	void operator=(Rs485 const &) = delete;

	cs_err_t init(struct cs_uart_config *cfg);

private:
	Rs485() {}
	~Rs485();
	static void handle_uart_event(const struct device *dev, struct uart_event *evt,
				      void *user_data);
	static void handle_rx_buf_rq(const struct device *dev, struct uart_event *evt);
	static void handle_uart_data(const struct device *dev, struct uart_event *evt);

	bool _isInitialized = false;

	uint8_t *_rx_msg_buf = NULL;
	uint8_t *_rx_msg_buf_ptr = NULL;
	uint16_t _rx_msg_buf_ctr = 0;
};