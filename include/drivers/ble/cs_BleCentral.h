/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "drivers/ble/cs_ServiceUuid.h"
#include "cs_ReturnTypes.h"

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/addr.h>
#include <zephyr/bluetooth/gatt.h>

#define CS_BLE_CENTRAL_BUFFER_SIZE 256

#define CS_BLE_CENTRAL_CONN_TIMEOUT	    400
#define CS_BLE_CENTRAL_ADDR_TYPE_RANDOM_STR "random"
#define CS_BLE_CENTRAL_GATT_WRITE_OVERHEAD  3

#define CS_BLE_CENTRAL_AVAILABLE_EVENT 1
#define CS_BLE_CENTRAL_CONNECTED_EVENT 2

class BleCentral
{
      public:
	static BleCentral &getInstance()
	{
		static BleCentral instance;
		return instance;
	}
	// Deny implementation
	BleCentral(BleCentral const &) = delete;
	BleCentral(BleCentral &&) = delete;
	void operator=(BleCentral const &) = delete;
	void operator=(BleCentral &&) = delete;

	cs_ret_code_t init();
	cs_ret_code_t connect(const char *device_addr);
	cs_ret_code_t discoverServices(ServiceUuid *uuid, bt_gatt_discover_func_t cb);
	cs_ret_code_t write(uint16_t handle, uint8_t *data, uint16_t len, bt_gatt_write_func_t cb);
	cs_ret_code_t read(uint16_t handle, bt_gatt_read_func_t cb);
	cs_ret_code_t read(uint16_t *handles, uint8_t handle_count, bt_gatt_read_func_t cb);
	cs_ret_code_t waitAvailable(int timeout_ms);
	cs_ret_code_t waitConnected(int timeout_ms);
	cs_ret_code_t disconnect();

	bool isInitialized();
	bool isConnected();

	/** Event to notify that the instance is ready for a new connection */
	k_event _ble_conn_evts;

	/** BT connection instance reference */
	bt_conn *_conn = NULL;

	/** BT connection create parameters */
	bt_conn_le_create_param _conn_create_params;
	/** BT connection initial parameters instance */
	bt_le_conn_param _conn_init_params;
	/** BT connection scan parameters */
	bt_le_scan_param _scan_params;

	/** BT MTU exchange params */
	bt_gatt_exchange_params _gatt_exchange_params;
	/** BT GATT discover params */
	bt_gatt_discover_params _gatt_discover_params;
	/** UUID used for discovery fitlering */
	cs_ble_uuid _discover_uuid;

	/** BT connection callbacks */
	bt_conn_cb _conn_cbs;
	/** BT GATT callbacks */
	bt_gatt_cb _gatt_cbs;

	/** MAC address of device to connect to */
	bt_addr_le_t _dev_addr;

	/** BT GATT write params */
	bt_gatt_write_params _gatt_write_params;
	/** BT GATT read params */
	bt_gatt_read_params _gatt_read_params;

	/** BLE write buffer of 256 bytes */
	uint8_t _ble_write_buf[CS_BLE_CENTRAL_BUFFER_SIZE];
	/** Counter for the amount of bytes currently in the BLE write buffer */
	uint16_t _ble_write_buf_ctr = 0;

      private:
	BleCentral() = default;

	/** Initialized flag */
	bool _initialized = false;
};