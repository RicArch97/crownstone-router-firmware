/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Uart.h"
#include "drivers/cs_Wifi.h"
#include "drivers/ble/cs_BleCentral.h"
#include "socket/cs_WebSocket.h"
#include "cs_ReturnTypes.h"
#include "cs_PacketHandling.h"
#include "cs_RouterProtocol.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Router, LOG_LEVEL_INF);

#include <zephyr/device.h>

#define RS485_DEVICE DT_NODELABEL(uart2)

#define TEST_SSID "ssid"
#define TEST_PSK  "psk"

#define HOST_ADDR "ip"
#define HOST_PORT 14500

#define CROWNSTONE_UUID "24f000007d104805bfc17663a01c3bff"

int main(void)
{
	cs_ret_code_t ret = CS_OK;

	PacketHandler pkt_handler;
	ret |= pkt_handler.init();

	Wifi *wifi = Wifi::getInstance();
	if (wifi->init(TEST_SSID, TEST_PSK) == CS_OK) {
		int conn_ret = CS_OK;
		do {
			conn_ret = wifi->connect();
		} while (conn_ret == CS_ERR_WIFI_SCAN_RESULT_TIMEOUT);
	}

	// wait till wifi connection is established before creating websocket
	ret |= wifi->waitConnected(SYS_FOREVER_MS);

	WebSocket web_socket(CS_INSTANCE_ID_CLOUD, &pkt_handler);
	ret |= web_socket.init(HOST_ADDR, CS_SOCKET_IPV4, HOST_PORT);
	ret |= pkt_handler.registerHandler(CS_INSTANCE_ID_CLOUD, &web_socket,
					   WebSocket::sendMessage);
	ret |= web_socket.connect(NULL);

	BleCentral *ble = BleCentral::getInstance();
	ble->setSourceId(CS_INSTANCE_ID_BLE_CROWNSTONE_PERIPHERAL);
	ble->setDestinationId(CS_INSTANCE_ID_CLOUD);
	ret |= ble->init(CROWNSTONE_UUID, &pkt_handler);
	ret |= pkt_handler.registerHandler(CS_INSTANCE_ID_BLE_CROWNSTONE_PERIPHERAL, &ble,
					   BleCentral::sendBleMessage);

	const device *rs485_dev = DEVICE_DT_GET(RS485_DEVICE);
	Uart rs485(rs485_dev, CS_INSTANCE_ID_UART_RS485, CS_INSTANCE_ID_CLOUD, &pkt_handler);
	ret |= rs485.init(NULL);
	ret |= pkt_handler.registerHandler(CS_INSTANCE_ID_UART_RS485, &rs485,
					   Uart::sendUartMessage);

	if (ret) {
		LOG_ERR("Failed to initialize router (err %d)", ret);
		return EXIT_FAILURE;
	}

	LOG_INF("%s", "Crownstone router initialized");

	k_sleep(K_FOREVER);

	return EXIT_SUCCESS;
}