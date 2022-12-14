/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Router, LOG_LEVEL_INF);

#include <zephyr/device.h>

#include "cs_ReturnTypes.h"
#include "cs_Router.h"
#include "drivers/cs_Uart.h"
#include "drivers/network/cs_Wifi.h"

// uart0 is assigned to uart-shell / console for monitoring
#define RS485_DEVICE DT_NODELABEL(uart1)
#define RS232_DEVICE DT_NODELABEL(uart2)

#define TEST_SSID "crownstonerouter"
#define TEST_PSK  "test1234"

int main(void)
{
	Wifi *wifi = &Wifi::getInstance();
	if (wifi->init(TEST_SSID, TEST_PSK) == CS_OK) {
		LOG_INF("Wifi initialized");
	}
	if (wifi->connect() == CS_OK) {
		LOG_INF("Wifi connection request done successfully");
	}

	const struct device *rs485_dev = DEVICE_DT_GET(RS485_DEVICE);
	Uart rs485(rs485_dev, CS_SOURCE_ID_UART_RS485);
	if (rs485.init(NULL) == CS_OK) {
		LOG_INF("Rs485 initialized");
	}

	LOG_INF("Crownstone router initialized");

	return 0;
}