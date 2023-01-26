/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "drivers/ble/cs_ServiceUuid.h"
#include "cs_PacketHandling.h"
#include "cs_RouterProtocol.h"
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

#define CS_BLE_CENTRAL_THREAD_PRIORITY	 K_PRIO_PREEMPT(7)
#define CS_BLE_CENTRAL_THREAD_STACK_SIZE 4096

#define CS_BLE_CENTRAL_BUFFER_SIZE 256
#define CS_BLE_CENTRAL_QUEUE_SIZE  2

enum cs_characteristics_ids {
	SESSION_DATA_UUID = 0xE,
	CONTROL_UUID = 0xC,
	RESULT_UUID = 0xD,
};

class BleCentral
{
      public:
	static BleCentral *getInstance()
	{
		static BleCentral instance;
		return &instance;
	}
	// Deny implementation
	BleCentral(BleCentral const &) = delete;
	BleCentral(BleCentral &&) = delete;
	void operator=(BleCentral const &) = delete;
	void operator=(BleCentral &&) = delete;

	cs_ret_code_t init(const char *base_uuid, PacketHandler *pkt_handler);
	cs_ret_code_t connect(const char *device_addr);
	cs_ret_code_t discoverServices(ServiceUuid *uuid);
	cs_ret_code_t write(uint16_t handle, uint8_t *data, uint16_t len);
	cs_ret_code_t read(uint16_t handle);
	cs_ret_code_t waitAvailable(int timeout_ms);
	cs_ret_code_t disconnect();

	static void sendBleMessage(k_work *item);

	bool isInitialized();
	bool isConnected();

	inline void setSourceId(cs_router_instance_id src_id)
	{
		_src_id = src_id;
	}
	inline void setDestinationId(cs_router_instance_id dest_id)
	{
		_dest_id = dest_id;
	}

	/** Identifier for the connected device */
	cs_router_instance_id _src_id = CS_INSTANCE_ID_UNKNOWN;
	/** Identifier for where data from the device should be send to */
	cs_router_instance_id _dest_id = CS_INSTANCE_ID_UNKNOWN;
	/** PacketHandler instance to handle packet handling / transport */
	PacketHandler *_pkt_handler = NULL;

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
	/** BT GATT subscribe params */
	bt_gatt_subscribe_params _gatt_subscribe_params;

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

	/** BLE buffer of 256 bytes */
	uint8_t _ble_buf[CS_BLE_CENTRAL_BUFFER_SIZE];
	/** Counter for the amount of bytes currently in the BLE write buffer */
	uint16_t _ble_buf_ctr = 0;

	/** Base UUID used for discovery */
	ServiceUuid _uuid_base;
	/** GATT CCC UUID for descriptor discovery */
	ServiceUuid _uuid_ccc;

	/** Chacracterisctic count to make sure we checked all */
	uint8_t _chrc_handle_count = 0;

	uint16_t _sessionDataHandle = 0;
	/** Handle used to control the device */
	uint16_t _controlHandle = 0;
	/** Handle used to retrieve a result from the device */
	uint16_t _resultHandle = 0;
	/** Handle used to read characteristics */
	uint16_t _resultCccdHandle = 0;

      private:
	BleCentral() = default;

	/** Initialized flag */
	bool _initialized = false;
};