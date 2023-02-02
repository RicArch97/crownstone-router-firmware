/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

#include <stdint.h>
#include <stdbool.h>

#define CS_WIFI_SSID_FOUND_EVENT 1
#define CS_WIFI_CONNECTED_EVENT	 2
// timeouts in ms
#define CS_WIFI_SCAN_TIMEOUT	 5000

class Wifi
{
      public:
	static Wifi *getInstance()
	{
		static Wifi instance;
		return &instance;
	}
	// Deny implementation
	Wifi(Wifi const &) = delete;
	Wifi(Wifi &&) = delete;
	void operator=(Wifi const &) = delete;
	void operator=(Wifi &&) = delete;

	cs_ret_code_t init(const char *ssid, const char *psk);
	cs_ret_code_t connect();
	cs_ret_code_t waitConnected(uint16_t timeout_ms);
	cs_ret_code_t disconnect();

	/** SSID buffer, max 32 bytes (characters) */
	uint8_t _ssid[WIFI_SSID_MAX_LEN];
	/** Length of the provided SSID */
	uint8_t _ssid_len = 0;
	/** PSK buffer, max 64 bytes (characters) */
	uint8_t _psk[WIFI_PSK_MAX_LEN];
	/** Length of the provided PSK */
	uint8_t _psk_len = 0;

	/** Callback for wifi events */
	net_mgmt_event_callback _wifi_mgmt_cb;
	/** Callback for network events */
	net_mgmt_event_callback _dhcp_mgmt_cb;

	/** Structure with parameters for a wifi connect request */
	wifi_connect_req_params _cnx_params;

	/** Event structure used for wifi events */
	k_event _wifi_evts;

	/** Flag used to indicate that a disconnection has been requested */
	bool _disconnecting = false;

      private:
	Wifi() = default;

	/** Initialized flag */
	bool _initialized = false;

	/** Network interface structure */
	net_if *_iface = NULL;
};
