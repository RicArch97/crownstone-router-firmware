/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Uart.h"
#include "drivers/cs_Wifi.h"
#include "drivers/ble/cs_CrownstoneCentral.h"
#include "socket/cs_WebSocket.h"
#include "cs_ReturnTypes.h"
#include "cs_PacketHandling.h"
#include "cs_Router.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Router, LOG_LEVEL_INF);

#include <zephyr/device.h>

#define RS485_DEVICE DT_NODELABEL(uart2)

#define TEST_SSID "ssid"
#define TEST_PSK  "psk"

#define HOST_ADDR "addr"
#define HOST_PORT 14500

#define CROWNSTONE_MAC "mac"

int main(void)
{
	Wifi *wifi = &Wifi::getInstance();
	if (wifi->init(TEST_SSID, TEST_PSK) == CS_OK) {
		LOG_INF("%s", "Wifi initialized");
		// retry if scan result did not contain our SSID
		int conn_ret = CS_OK;
		do {
			conn_ret = wifi->connect();
		} while (conn_ret == CS_ERR_WIFI_SCAN_RESULT_TIMEOUT);
		LOG_INF("%s", "Wifi connection request done");
	}

	// wait till wifi connection is established before creating websocket
	k_event_wait(&wifi->_wifi_evts, CS_WIFI_CONNECTED_EVENT, true, K_FOREVER);

	PacketHandler pkt_handler;
	pkt_handler.init();

	const device *rs485_dev = DEVICE_DT_GET(RS485_DEVICE);
	Uart rs485(rs485_dev, CS_INSTANCE_ID_UART_RS485, CS_INSTANCE_ID_CLOUD, &pkt_handler);
	WebSocket web_socket(&pkt_handler);

	// register instances with transport callbacks
	pkt_handler.registerTransportHandler(CS_INSTANCE_ID_UART_RS485, &rs485,
					     Uart::sendUartMessage);
	pkt_handler.registerTransportHandler(CS_INSTANCE_ID_CLOUD, &web_socket,
					     WebSocket::sendMessage);

	if (web_socket.init(HOST_ADDR, CS_SOCKET_IPV4, HOST_PORT) == CS_OK) {
		LOG_INF("%s", "Websocket initialized");
		web_socket.connect(NULL);
	}

	if (rs485.init(NULL) == CS_OK) {
		LOG_INF("%s", "RS485 initialized");
	}

	CrownstoneCentral *crwn = &CrownstoneCentral::getInstance();
	if (crwn->init() == CS_OK) {
		LOG_INF("%s", "Crownstone central initialized");
		// connect to given (currently hardcoded) MAC address
		crwn->connect(CROWNSTONE_MAC);
	}

	LOG_INF("%s", "Crownstone router initialized");

	k_sleep(K_FOREVER);

	return 0;
}