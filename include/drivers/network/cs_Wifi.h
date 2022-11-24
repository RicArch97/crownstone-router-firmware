/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <zephyr/net/net_core.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>

#include "cs_ReturnTypes.h"

#define SSID_FOUND_EVENT  0x001
// timeouts in ms
#define SCAN_TIMEOUT	   5000
#define CONNECT_TIMOUT	   10000

class Wifi
{
public:
	static Wifi &getInstance()
	{
		static Wifi instance;
		return instance;
	}
	Wifi(Wifi const &) = delete;
	void operator=(Wifi const &) = delete;

	cs_err_t init(const char *ssid, const char *psk);
	cs_err_t connect();
	cs_err_t disconnect();

	uint8_t _ssid[WIFI_SSID_MAX_LEN], _ssid_len = 0;
	uint8_t _psk[WIFI_PSK_MAX_LEN], _psk_len = 0;

private:
	Wifi() {}
	~Wifi();
	// callbacks
	static void handle_wifi_result(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
				       struct net_if *iface);
	static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb);
	static void handle_wifi_connection_result(struct net_mgmt_event_callback *cb,
						  bool disconnect);

	bool _isInitialized = false;
	struct net_if *_iface = NULL;
	struct wifi_connect_req_params _cnx_params = {0};
};
