/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Wifi.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Wifi, LOG_LEVEL_INF);

#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include <string.h>

#define WIFI_MODULE DT_NODELABEL(wifi)
#define WIFI_MGMT_EVENTS                                                                           \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |   \
	 NET_EVENT_WIFI_DISCONNECT_RESULT)

/**
 * @brief Handle wifi scan result.
 */
static void handleWifiScanResult(net_mgmt_event_callback *cb)
{
	const wifi_scan_result *entry = (const wifi_scan_result *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (memcmp(wifi_inst->_ssid, entry->ssid, wifi_inst->_ssid_len) == 0) {
		wifi_inst->_cnx_params.security = entry->security;
		wifi_inst->_cnx_params.band = entry->band;
		wifi_inst->_cnx_params.channel = entry->channel;
		wifi_inst->_cnx_params.mfp = entry->mfp;

		// post event once given ssid was matched
		k_event_post(&wifi_inst->_wifi_evts, CS_WIFI_SSID_FOUND_EVENT);
	}
}

/**
 * @brief Handle wifi connect result.
 */
static void handleWifiConnectionResult(net_mgmt_event_callback *cb)
{
	const wifi_status *status = (const wifi_status *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (status->status) {
		LOG_ERR("Connection request failed (%d)", status->status);
	} else {
		k_event_post(&wifi_inst->_wifi_evts, CS_WIFI_CONNECTED_EVENT);
		LOG_INF("%s", "Connected");
	}
}

/**
 * @brief Handle wifi disconnect result.
 */
static void handleWifiDisconnectionResult(net_mgmt_event_callback *cb)
{
	const wifi_status *status = (const wifi_status *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (wifi_inst->_disconnecting) {
		if (status->status) {
			LOG_ERR("Disconnection request failed (%d)", status->status);
		} else {
			LOG_INF("Disconnection request done (%d)", status->status);
		}
	} else {
		LOG_INF("%s", "Disconnected");
	}
}

/**
 * @brief Handle wifi events.
 */
static void handleWifiResult(net_mgmt_event_callback *cb, uint32_t mgmt_event, net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handleWifiScanResult(cb);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handleWifiConnectionResult(cb);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handleWifiDisconnectionResult(cb);
		break;
	default:
		break;
	}
}

/**
 * @brief Initialize the wifi module.
 *
 * @param ssid SSID of the Wifi network.
 * @param psk Passkey of the Wifi network.
 *
 * @return CS_OK if the wifi module was sucessfully initialized.
 */
cs_ret_code_t Wifi::init(const char *ssid, const char *psk)
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	const device *wifi_dev = DEVICE_DT_GET(WIFI_MODULE);
	// obtain a device reference and check if device is ready
	if (!device_is_ready(wifi_dev)) {
		LOG_ERR("WiFi device %s is not ready", wifi_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

	_iface = net_if_lookup_by_dev(wifi_dev);
	// Get the default network interface
	if (_iface == NULL) {
		LOG_ERR("No interface with device %s is configured", _iface->if_dev->dev->name);
		return CS_ERR_INTERFACE_NOT_AVAILABLE;
	}

	// initialize and add wifi event handler
	static net_mgmt_event_callback wifi_mgmt_cb;
	net_mgmt_init_event_callback(&wifi_mgmt_cb, handleWifiResult, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	// init ssid found event and connected event
	k_event_init(&_wifi_evts);

	// clamp lengths to the max supported
	_ssid_len = (uint8_t)CLAMP(strlen(ssid), 0, WIFI_SSID_MAX_LEN);
	_psk_len = (uint8_t)CLAMP(strlen(psk), 0, WIFI_PSK_MAX_LEN);
	// store ssid and psk
	memcpy(_ssid, (uint8_t *)ssid, _ssid_len);
	memcpy(_psk, (uint8_t *)psk, _psk_len);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Scan wifi networks for given ssid, connect if scan matches given ssid.
 *
 * @return CS_OK if if the scan request was done succesfully.
 */
cs_ret_code_t Wifi::connect()
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, _iface, NULL, 0) != 0) {
		LOG_ERR("%s", "Scan request failed");
		return CS_ERR_WIFI_SCAN_REQUEST_FAILED;
	}

	// it's possible the scan did not detect the given ssid
	// return so connection can be reattempted
	if (k_event_wait(&_wifi_evts, CS_WIFI_SSID_FOUND_EVENT, true,
			 K_MSEC(CS_WIFI_SCAN_TIMEOUT)) == 0) {
		LOG_WRN("%s", "Timeout on waiting for scan result");
		return CS_ERR_WIFI_SCAN_RESULT_TIMEOUT;
	}

	_cnx_params.ssid = _ssid;
	_cnx_params.ssid_length = _ssid_len;
	_cnx_params.psk = _psk;
	_cnx_params.psk_length = _psk_len;
	_cnx_params.timeout = CS_WIFI_CONNECT_TIMOUT;

	LOG_DBG("ssid: %-32s | channel: %-4u band: (%-6s) | security: %-15s | mpf: %-9s",
		_cnx_params.ssid, _cnx_params.channel,
		wifi_band_txt((wifi_frequency_bands)_cnx_params.band),
		wifi_security_txt(_cnx_params.security), wifi_mfp_txt(_cnx_params.mfp));

	if (net_mgmt(NET_REQUEST_WIFI_CONNECT, _iface, &_cnx_params,
		     sizeof(wifi_connect_req_params)) != 0) {
		LOG_ERR("%s", "Wifi connect request failed");
	}

	return CS_OK;
}

/**
 * @brief Disconnect from wifi network.
 *
 * @return CS_OK if the disconnect request was done sucessfully.
 */
cs_ret_code_t Wifi::disconnect()
{
	_disconnecting = true;

	if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, _iface, NULL, 0) != 0) {
		LOG_ERR("%s", "Disconnect request failed");
		return CS_ERR_WIFI_DISCONNECT_REQUEST_FAILED;
	}

	return CS_OK;
}