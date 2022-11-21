/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
 */

#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Network, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>

#include "cs_ReturnTypes.h"
#include "network/cs_Wifi.h"

#define WIFI_MODULE DT_NODELABEL(wifi)
#define WIFI_MGMT_EVENTS                                                                           \
	(NET_EVENT_WIFI_SCAN_RESULT | NET_EVENT_WIFI_SCAN_DONE | NET_EVENT_WIFI_CONNECT_RESULT |   \
	 NET_EVENT_WIFI_DISCONNECT_RESULT)

static struct k_mutex cb_mutex;
static struct k_event wifi_scan_event;

/**
 * @brief Initialize the Network module.
 *
 * @return CS_OK if the wifi module was sucessfully initialized.
 */
cs_err_t Wifi::init(const uint8_t *ssid, uint8_t ssid_len, const uint8_t *psk, uint8_t psk_len)
{
	if (_isInitialized) {
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
	net_mgmt_init_event_callback(&wifi_mgmt_cb, handle_wifi_result, WIFI_MGMT_EVENTS);
	net_mgmt_add_event_callback(&wifi_mgmt_cb);

	// init mutex and scan result received event
	k_mutex_init(&cb_mutex);
	k_event_init(&wifi_scan_event);

	// store ssid and psk
	_ssid_len = CLAMP(ssid_len, 0, WIFI_SSID_MAX_LEN);
	_psk_len = CLAMP(psk_len, 0, WIFI_PSK_MAX_LEN);
	memcpy(_ssid, ssid, _ssid_len);
	memcpy(_psk, psk, _psk_len);

	_isInitialized = true;

	return CS_OK;
}

/**
 * @brief Scan wifi networks for given ssid, connect if scan matches given ssid.
 *
 * @return CS_OK if if the scan request was done succesfully.
 */
cs_err_t Wifi::connect()
{
	if (!_isInitialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (net_mgmt(NET_REQUEST_WIFI_SCAN, _iface, NULL, 0) != 0) {
		LOG_ERR("Scan request failed");
		return CS_ERR_WIFI_SCAN_REQUEST_FAILED;
	}

	if (k_event_wait(&wifi_scan_event, SCAN_RESULT_EVENT, true, K_MSEC(SCAN_TIMEOUT)) == 0) {
		LOG_ERR("Timeout on waiting for scan result");
		return CS_ERR_WIFI_SCAN_RESULT_TIMEOUT;
	}

	_cnx_params.ssid = _ssid;
	_cnx_params.ssid_length = _ssid_len;
	_cnx_params.psk = _psk;
	_cnx_params.psk_length = _psk_len;
	_cnx_params.timeout = CONNECT_TIMOUT;

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
	if (net_mgmt(NET_REQUEST_WIFI_DISCONNECT, _iface, NULL, 0) != 0) {
		LOG_ERR("Disconnect request failed");
		return CS_ERR_WIFI_DISCONNECT_REQUEST_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Handle wifi events.
 */
void Wifi::handle_wifi_result(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
			      struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_CONNECT_RESULT:
		handle_wifi_connection_result(cb, false);
		break;
	case NET_EVENT_WIFI_DISCONNECT_RESULT:
		handle_wifi_connection_result(cb, true);
		break;
	default:
		break;
	}
}

/**
 * @brief Handle wifi scan result.
 */
void Wifi::handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry = (const struct wifi_scan_result *)cb->info;

	// lock mutex to safely access singleton instance
	if (k_mutex_lock(&cb_mutex, K_MSEC(MUTEX_LOCK_TIMEOUT)) != 0) {
		LOG_ERR("Failed to lock access to Wifi instance");
		return;
	}

	Wifi *wifi = &Wifi::getInstance();
	if (memcmp(wifi->_ssid, entry->ssid, wifi->_ssid_len) == 0) {
		wifi->_cnx_params.security = entry->security;
		wifi->_cnx_params.band = entry->band;
		wifi->_cnx_params.channel = entry->channel;
		wifi->_cnx_params.mfp = entry->mfp;
	}

	k_mutex_unlock(&cb_mutex);
	k_event_post(&wifi_scan_event, SCAN_RESULT_EVENT);
}

/**
 * @brief Handle wifi connect result.
 */
void Wifi::handle_wifi_connection_result(struct net_mgmt_event_callback *cb, bool disconnect)
{
	const struct wifi_status *status = (const struct wifi_status *)cb->info;
	const char *msg = disconnect ? "Disconnection" : "Connection";

	if (status->status) {
		LOG_ERR("%s unsuccesful", msg);
	} else {
		LOG_INF("%s succussful!", msg);
	}
}

/**
 * @brief Free all heap allocated memory.
 */
Wifi::~Wifi()
{
	k_free(_iface);
}