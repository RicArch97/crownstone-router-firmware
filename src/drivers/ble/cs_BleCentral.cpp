/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Jan., 2023
 * License: Apache License 2.0
 */

#include "drivers/ble/cs_BleCentral.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_BleCentral, LOG_LEVEL_INF);

#include <zephyr/kernel.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>

/**
 * @brief Handle BLE Extended Inquiry Response (EIR)
 */
static bool handleBleEirReceived(struct bt_data *data, void *user_data)
{
	bt_addr_le_t *addr = static_cast<bt_addr_le_t *>(user_data);

	//LOG_INF("BLE data type: %hhu", data->type);

	bt_le_scan_stop();

	return true;
}

/**
 * @brief Handle BLE device found.
 */
static void handleBleDeviceFound(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
				 struct net_buf_simple *ad)
{
	char dev[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(addr, dev, sizeof(dev));
	LOG_DBG("Bluetooth device found: %s, RSSI %i", dev, rssi);

	// Check for connectable events
	if (type == BT_GAP_ADV_TYPE_ADV_IND || type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		bt_data_parse(ad, handleBleEirReceived, (void *)addr);
	}
}

static void handleConnectionResult(struct bt_conn *conn, uint8_t conn_err)
{
	printk("not implemented\n");
}

static void handleDisonnectionResult(struct bt_conn *conn, uint8_t reason)
{
	printk("not implemented\n");
}

/**
 * @brief Initialize Bluetooth controller
 */
cs_ret_code_t BleCentral::init()
{
	int ret;
	// synchronously enable bluetooth controller
	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("Bluetooth init failed (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_BLUETOOTH_INIT_FAILED;
	}

	LOG_INF("%s", "Bluetooth initialized");

	memset(&_conn_cbs, 0, sizeof(_conn_cbs));
	_conn_cbs.connected = handleConnectionResult;
	_conn_cbs.disconnected = handleDisonnectionResult;

	bt_conn_cb_register(&_conn_cbs);

	return CS_OK;
}

/**
 * @brief Connect to a device with given MAC address.
 * A BLE scan is initiatiated to scan for the device first.
 */
cs_ret_code_t BleCentral::connect()
{
	struct bt_le_scan_param scan_params;
	memset(&scan_params, 0, sizeof(scan_params));

	scan_params.type = BT_LE_SCAN_TYPE_ACTIVE;
	scan_params.options = BT_LE_SCAN_OPT_NONE;
	scan_params.interval = BT_GAP_SCAN_FAST_INTERVAL;
	scan_params.window = BT_GAP_SCAN_FAST_WINDOW;

	int ret = bt_le_scan_start(&scan_params, handleBleDeviceFound);
	if (ret) {
		LOG_ERR("Failed to start BLE scan (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_SCAN_START_FAILED;
	}

	LOG_INF("%s", "Started BLE scan");

	return CS_OK;
}