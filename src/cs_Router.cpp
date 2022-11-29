/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Router, LOG_LEVEL_INF);

#include "cs_ReturnTypes.h"
#include "drivers/network/cs_Wifi.h"
#include "drivers/uart/cs_Rs485.h"

#define TEST_SSID "crownstonerouter"
#define TEST_PSK  "test1234"

int main(void)
{
	// Wifi *wifi = &Wifi::getInstance();

	// if (wifi->init(TEST_SSID, TEST_PSK) == CS_OK) {
	// 	LOG_INF("Wifi initialized");
	// }

	// if (wifi->connect() == CS_OK) {
	// 	LOG_INF("Wifi connection request done successfully");
	// }

	struct cs_uart_config cfg = {
		.baudrate = 115200
	};

	Rs485 *rs485 = &Rs485::getInstance();

	if (rs485->init(&cfg) == CS_OK) {
		LOG_INF("Rs485 initialized");
	}

	LOG_INF("Crownstone router initialized");

	return 0;
}