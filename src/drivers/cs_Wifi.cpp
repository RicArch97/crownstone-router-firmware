/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include "drivers/cs_Wifi.h"
#include "cs_ReturnTypes.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Wifi, LOG_LEVEL_DBG);

#include <zephyr/kernel.h>
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
static void handleWifiScanResult(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry = (const struct wifi_scan_result *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (memcmp(wifi_inst->_ssid, entry->ssid, wifi_inst->_ssid_len) == 0) {
		wifi_inst->_cnx_params.security = entry->security;
		wifi_inst->_cnx_params.band = entry->band;
		wifi_inst->_cnx_params.channel = entry->channel;
		wifi_inst->_cnx_params.mfp = entry->mfp;

		// post event once given ssid was matched
		k_event_set(&wifi_inst->_evt_ssid_found, CS_WIFI_SSID_FOUND_EVENT);
	}
}

/**
 * @brief Handle wifi connect result.
 */
static void handleWifiConnectionResult(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (status->status) {
		LOG_ERR("Connection request failed (%d)", status->status);
	} else {
		k_event_set(&wifi_inst->_evt_connected, CS_WIFI_CONNECTED_EVENT);
		LOG_INF("Connected");
	}
}

/** 
 * @brief Handle wifi disconnect result.
*/
static void handleWifiDisconnectionResult(struct net_mgmt_event_callback *cb)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;

	// get singleton instance
	Wifi *wifi_inst = &Wifi::getInstance();

	if (wifi_inst->_disconnecting) {
		if (status->status) {
			LOG_ERR("Disconnection request failed (%d)", status->status);
		}
		else {
			LOG_INF("Disconnection request done (%d)", status->status);
		}
	}
	else {
		LOG_INF("Disconnected");
	}
}

/**
 * @brief Handle wifi events.
 */
static void handleWifiResult(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			     struct net_if *iface)
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
cs_err_t Wifi::init(const char *ssid, const char *psk)
{
	if (_initialized) {
		LOG_ERR("Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	const struct device *wifi_dev = DEVICE_DT_GET(WIFI_MODULE);
	// obtain a device reference and check if device is ready
	if (!device_is_ready(wifi_dev)) {
		LOG_ERR("WiFi device %s is not ready", wifi_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

	_iface = net_if_get_default();
	// Get the default network interface
	if (_iface == NULL) {
		LOG_ERR("No interface with device %s is configured", _iface->if_dev->dev->name);
		return CS_ERR_INTERFACE_NOT_AVAILABLE;
	}

	// initialize and add wifi event handler
	static struct net_mgmt_event_callback wifi_mgmt_cb;
	net_mgmt_init_event_callback(&wifi_mgmt_cb, handleWifiResult, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	// init ssid found event and connected event
	k_event_init(&_evt_ssid_found);
	k_event_init(&_evt_connected);

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
cs_err_t Wifi::connect()
{
	if (!_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, _iface, NULL, 0) != 0) {
		LOG_ERR("Scan request failed");
		return CS_ERR_WIFI_SCAN_REQUEST_FAILED;
	}

	// it's possible the scan did not detect the given ssid
	// return so connection can be reattempted
	if (k_event_wait(&_evt_ssid_found, CS_WIFI_SSID_FOUND_EVENT, true,
			 K_MSEC(CS_WIFI_SCAN_TIMEOUT)) == 0) {
		LOG_ERR("Timeout on waiting for scan result");
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
		     sizeof(struct wifi_connect_req_params)) != 0) {
		LOG_ERR("Wifi connect request failed");
	}

	return CS_OK;
}

/**
 * @brief Disconnect from wifi network.
 *
 * @return CS_OK if the disconnect request was done sucessfully.
 */
cs_err_t Wifi::disconnect()
{
	_disconnecting = true;

	if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, _iface, NULL, 0) != 0) {
		LOG_ERR("Disconnect request failed");
		return CS_ERR_WIFI_DISCONNECT_REQUEST_FAILED;
	}

	return CS_OK;
}