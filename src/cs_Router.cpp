/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Uart.h"
#include "drivers/cs_Wifi.h"
#include "socket/cs_WebSocket.h"
#include "cs_ReturnTypes.h"
#include "cs_Router.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Router, LOG_LEVEL_INF);

#include <zephyr/device.h>

#define RS485_DEVICE DT_NODELABEL(uart2)

#define TEST_SSID "ssid"
#define TEST_PSK  "psk"

#define HOST_ADDR "addr"
#define HOST_PORT 8080
#define HOST_NAME "CrownstoneRouter"

int main(void)
{
	Wifi *wifi = &Wifi::getInstance();
	if (wifi->init(TEST_SSID, TEST_PSK) == CS_OK) {
		LOG_INF("Wifi initialized");
		if (wifi->connect() == CS_OK) {
			LOG_INF("Wifi connection request done");
		}
	}

	// wait till wifi connection is established before creating websocket
	k_event_wait(&wifi->_evt_connected, CS_WIFI_CONNECTED_EVENT, true, K_FOREVER);
	
	struct cs_socket_host_addr ws_host_addr;
	ws_host_addr.peer_addr = HOST_ADDR;
	ws_host_addr.port = HOST_PORT;
	ws_host_addr.host_name = HOST_NAME;

	struct cs_socket_opts ws_opts;
	ws_opts.host_mode = CS_SOCKET_HOST_ADDR;
	ws_opts.ip_ver = CS_SOCKET_IPV4;
	ws_opts.addr = &ws_host_addr;

	WebSocket ws;
	if (ws.init(&ws_opts) == CS_OK) {
		LOG_INF("Websocket initialized");
		ws.connect(NULL);
	}

	const struct device *rs485_dev = DEVICE_DT_GET(RS485_DEVICE);
	Uart rs485(rs485_dev, CS_SOURCE_ID_UART_RS485, &ws);
	if (rs485.init(NULL) == CS_OK) {
		LOG_INF("RS485 initialized");
	}

	LOG_INF("Crownstone router initialized");

	k_sleep(K_FOREVER);

	return 0;
}