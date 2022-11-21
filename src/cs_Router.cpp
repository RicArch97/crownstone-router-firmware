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
#include "network/cs_Wifi.h"

#define TEST_SSID "ssid"
#define TEST_PSK  "psk"

int main(void)
{
	Wifi *wifi = &Wifi::getInstance();

	if (wifi->init((uint8_t *)TEST_SSID, strlen(TEST_SSID), (uint8_t *)TEST_PSK,
		       strlen(TEST_PSK)) == CS_OK) {
		LOG_INF("Crownstone router initialized");
	}

	if (wifi->connect() == CS_OK) {
		LOG_INF("Wifi connection request done successfully");
	}

	return 0;
}