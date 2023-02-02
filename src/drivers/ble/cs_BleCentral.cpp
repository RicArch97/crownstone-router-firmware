/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Jan., 2023
 * License: Apache License 2.0
 */

#include "drivers/ble/cs_BleCentral.h"
#include "cs_PacketHandling.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_BleCentral, LOG_LEVEL_INF);

#include <zephyr/kernel.h>

/**
 * @brief Handle notifications.
 */
static uint8_t handleNotifications(bt_conn *conn, bt_gatt_subscribe_params *params,
				   const void *data, uint16_t length)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	if (!conn || !data) {
		return BT_GATT_ITER_STOP;
	}

	uint8_t *part = (uint8_t *)data;
	uint8_t counter = part[0];

	if ((ble_inst->_ble_buf_ctr + length - 1U) > sizeof(ble_inst->_ble_buf)) {
		LOG_ERR("%s", "Failed to parse notification, length exceeds buffer size");
		return BT_GATT_ITER_STOP;
	}
	// add data to buffer, notification comes in chunks (first byte is counter)
	memcpy(ble_inst->_ble_buf + ble_inst->_ble_buf_ctr, part + 1, length - 1);
	ble_inst->_ble_buf_ctr += length - 1;

	if (counter == UINT8_MAX) {
		LOG_HEXDUMP_DBG(ble_inst->_ble_buf, ble_inst->_ble_buf_ctr, "Notification");

		cs_packet_data ble_notif_data;
		memset(&ble_notif_data, 0, sizeof(ble_notif_data));
		ble_notif_data.msg.buf_len = ble_inst->_ble_buf_ctr;
		ble_notif_data.src_id = ble_inst->_src_id;
		ble_notif_data.dest_id = ble_inst->_dest_id;
		ble_notif_data.type = CS_DATA_OUTGOING;
		ble_notif_data.result_code = CS_RESULT_TYPE_SUCCES;
		memcpy(&ble_notif_data.msg.buf, ble_inst->_ble_buf, ble_inst->_ble_buf_ctr);

		if (ble_inst->_pkt_handler != NULL) {
			// data is copied into work handler, so we don't have to save the struct
			ble_inst->_pkt_handler->handlePacket(&ble_notif_data);
		} else {
			LOG_WRN("%s", "Failed to handle BLE notification");
		}

		ble_inst->_ble_buf_ctr = 0;
		ble_inst->disconnect();
		return BT_GATT_ITER_STOP;
	}

	return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Handle discovery done, read session data.
 */
static void handleDiscoveryDone(bt_conn *conn, uint8_t err, bt_gatt_subscribe_params *params)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	if (err) {
		LOG_ERR("Subscription to handle %d failed", params->value_handle);
		return;
	}

	ble_inst->read(ble_inst->_sessionDataHandle);
}

/**
 * @brief Handle discovery results.
 */
static uint8_t handleDiscoveryResults(bt_conn *conn, const bt_gatt_attr *attr,
				      bt_gatt_discover_params *params)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	if (params->type == BT_GATT_DISCOVER_PRIMARY) {
		if (attr == NULL) {
			// look for all characteristics for our service
			ble_inst->_gatt_discover_params.uuid = NULL;
			ble_inst->_gatt_discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
			ble_inst->_gatt_discover_params.start_handle = ble_inst->_next_handle;

			int ret = bt_gatt_discover(conn, &ble_inst->_gatt_discover_params);
			if (ret) {
				LOG_ERR("Failed to start GATT discovery (err %d)", ret);
			}
		} else {
			bt_gatt_service_val *srv_val =
				static_cast<bt_gatt_service_val *>(attr->user_data);

			ServiceUuid found_uuid(srv_val->uuid);
			if (found_uuid == ble_inst->_uuid_base) {
				ble_inst->_next_handle = attr->handle + 1;
				LOG_DBG("%s", "Discovered primary service");
			}
		}
	} else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
		if (attr == NULL) {
			// look for the CCC of the result characteristic
			ble_inst->_gatt_discover_params.uuid =
				&ble_inst->_uuid_ccc.getUuid()->uuid_16.uuid;
			ble_inst->_gatt_discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
			ble_inst->_gatt_discover_params.start_handle = ble_inst->_next_handle;
			// only one descriptor
			ble_inst->_gatt_discover_params.end_handle = ble_inst->_next_handle + 1;

			int ret = bt_gatt_discover(conn, &ble_inst->_gatt_discover_params);
			if (ret) {
				LOG_ERR("Failed to start GATT discovery (err %d)", ret);
			}
		} else {
			bt_gatt_chrc *chrc = static_cast<bt_gatt_chrc *>(attr->user_data);

			char uuid_str[BT_UUID_STR_LEN];
			bt_uuid_to_str(chrc->uuid, uuid_str, sizeof(uuid_str));
			LOG_DBG("Discovered UUID: %s", uuid_str);

			ServiceUuid found_uuid(chrc->uuid);
			ServiceUuid session_uuid, ctrl_uuid, result_uuid;

			session_uuid.fromBaseUuid(&ble_inst->_uuid_base, SESSION_DATA_UUID);
			ctrl_uuid.fromBaseUuid(&ble_inst->_uuid_base, CONTROL_UUID);
			result_uuid.fromBaseUuid(&ble_inst->_uuid_base, RESULT_UUID);

			if (found_uuid == session_uuid) {
				ble_inst->_sessionDataHandle = chrc->value_handle;
				LOG_DBG("Discovered Crownstone session data handle: %u",
					ble_inst->_sessionDataHandle);
			}
			if (found_uuid == ctrl_uuid) {
				ble_inst->_controlHandle = chrc->value_handle;
				LOG_DBG("Discovered Crownstone control handle: %u",
					ble_inst->_controlHandle);
			}
			if (found_uuid == result_uuid) {
				ble_inst->_resultHandle = chrc->value_handle;
				// we at this point we can still discover more characteristics
				ble_inst->_next_handle = attr->handle + 1;
				ble_inst->_gatt_subscribe_params.value_handle = chrc->value_handle;
				LOG_DBG("Discovered Crownstone result handle: %u",
					ble_inst->_resultHandle);
			}
		}
	} else {
		// no more results
		if (attr == NULL) {
			return BT_GATT_ITER_STOP;
		} else {
			ble_inst->_gatt_subscribe_params.notify = handleNotifications;
			ble_inst->_gatt_subscribe_params.subscribe = handleDiscoveryDone;
			ble_inst->_gatt_subscribe_params.value = BT_GATT_CCC_NOTIFY;
			ble_inst->_gatt_subscribe_params.ccc_handle = attr->handle;

			int ret = bt_gatt_subscribe(conn, &ble_inst->_gatt_subscribe_params);
			if (ret && ret != -EALREADY) {
				LOG_ERR("Subscribe failed (err %d)", ret);
			} else {
				LOG_DBG("Subscribed to handle: %hu",
					ble_inst->_gatt_subscribe_params.value_handle);
			}
			LOG_INF("%s", "Discovery completed.");

			return BT_GATT_ITER_STOP;
		}
	}
	return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Handle GATT write result.
 */
static void handleWriteResult(bt_conn *conn, uint8_t err, bt_gatt_write_params *params)
{
	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	if (err) {
		LOG_ERR("Failed to write %hu bytes to device %s with handle %hu (err %d)",
			params->length, dev, params->handle, err);
		return;
	}

	LOG_DBG("Wrote %hu bytes to device %s with handle %hu", params->length, dev,
		params->handle);
}

/**
 * @brief Handle GATT read result.
 */
static uint8_t handleReadResult(bt_conn *conn, uint8_t err, bt_gatt_read_params *params,
				const void *data, uint16_t length)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	if (!data) {
		LOG_HEXDUMP_DBG(ble_inst->_ble_buf, ble_inst->_ble_buf_ctr, "BLE read");

		cs_packet_data ble_read_data;
		memset(&ble_read_data, 0, sizeof(ble_read_data));
		ble_read_data.msg.buf_len = ble_inst->_ble_buf_ctr;
		ble_read_data.src_id = ble_inst->_src_id;
		ble_read_data.dest_id = ble_inst->_dest_id;
		ble_read_data.type = CS_DATA_OUTGOING;
		ble_read_data.result_code = CS_RESULT_TYPE_SUCCES;
		memcpy(&ble_read_data.msg.buf, ble_inst->_ble_buf, ble_inst->_ble_buf_ctr);

		if (ble_inst->_pkt_handler != NULL) {
			// data is copied into work handler, so we don't have to save the struct
			ble_inst->_pkt_handler->handlePacket(&ble_read_data);
		} else {
			LOG_WRN("%s", "Failed to handle BLE message");
		}

		ble_inst->_ble_buf_ctr = 0;
		LOG_DBG("%s", "Read completed.");
		return BT_GATT_ITER_STOP;
	}

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	if (err) {
		LOG_ERR("Failed to read %hu bytes from device %s with handle %hu (err %d)", length,
			dev, params->single.handle, err);
		return BT_GATT_ITER_STOP;
	}

	uint16_t offset = 0;
	if (params->single.offset > 0) {
		offset = params->single.offset;
	}
	if ((offset + length) > sizeof(ble_inst->_ble_buf)) {
		LOG_ERR("%s", "Read failed, message length exceeds buffer size");
		return BT_GATT_ITER_STOP;
	}
	// add data to buffer, in case the read data exceeds our MTU read is done in chunks
	memcpy(ble_inst->_ble_buf + offset, (uint8_t *)data, length);
	ble_inst->_ble_buf_ctr += length;

	return BT_GATT_ITER_CONTINUE;
}

/**
 * @brief Handle MTU exchange result.
 */
static void handleMtuExchangeResult(bt_conn *conn, uint8_t err, bt_gatt_exchange_params *params)
{
	LOG_DBG("MTU exchange %s (%u)", err == 0U ? "successful" : "failed", bt_gatt_get_mtu(conn));
}

/**
 * @brief Handle BLE device found.
 */
static void handleBleDeviceFound(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
				 net_buf_simple *ad)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(addr, dev, sizeof(dev));

	LOG_DBG("Bluetooth device found: %s, RSSI %i", dev, rssi);

	// only handle connectable events
	if (type != BT_GAP_ADV_TYPE_ADV_IND && type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
		return;
	}
	// only check the device we're interested in
	if (bt_addr_le_cmp(addr, &ble_inst->_dev_addr) != 0) {
		return;
	}

	int ret;
	// we have found the device, stop the scan
	ret = bt_le_scan_stop();
	if (ret) {
		LOG_ERR("Stop LE scan failed (err %d)", ret);
		return;
	}

	// initiate LE connection with the device
	ret = bt_conn_le_create(addr, &ble_inst->_conn_create_params, &ble_inst->_conn_init_params,
				&ble_inst->_conn);
	if (ret) {
		LOG_ERR("Failed to create LE connection instance (err %d)", ret);
		k_msleep(CS_BLE_CENTRAL_RECONNECT_TIMEOUT);
		ble_inst->connect(dev);
	}
}

/**
 * @brief Handle connection established to BLE device.
 */
static void handleConnectionResult(bt_conn *conn, uint8_t conn_err)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", dev, conn_err);

		bt_conn_unref(ble_inst->_conn);
		ble_inst->_conn = NULL;
		k_msleep(CS_BLE_CENTRAL_RECONNECT_TIMEOUT);
		ble_inst->connect(dev);
		return;
	}

	// indicate that the instance is now in use / connected
	k_event_clear(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);

	LOG_INF("Connected: %s", dev);

	ble_inst->_gatt_exchange_params.func = handleMtuExchangeResult;
	int ret = bt_gatt_exchange_mtu(conn, &ble_inst->_gatt_exchange_params);
	if (ret) {
		LOG_ERR("Failed to exchange MTU (err %d", ret);
		return;
	}

	// start discovering services based on the base UUID
	ble_inst->discoverServices(&ble_inst->_uuid_base);
}

/**
 * @brief Handle disconnection from BLE device.
 */
static void handleDisonnectionResult(bt_conn *conn, uint8_t reason)
{
	BleCentral *ble_inst = BleCentral::getInstance();

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	LOG_INF("Disconnected from BLE device: %s (reason 0x%02x)", dev, reason);

	if (ble_inst->_conn != conn) {
		return;
	}

	// cleanup accept list
	bt_le_filter_accept_list_clear();

	bt_conn_unref(ble_inst->_conn);
	ble_inst->_conn = NULL;
	// we are ready for a new connection again
	k_event_post(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);

	// if we didn't manually disconnect, retry
	if (reason != BT_HCI_ERR_REMOTE_USER_TERM_CONN &&
	    reason != BT_HCI_ERR_LOCALHOST_TERM_CONN) {
		k_msleep(CS_BLE_CENTRAL_RECONNECT_TIMEOUT);
		ble_inst->connect(dev);
	}
}

/**
 * @brief Handle MTU updated.
 */
void handleMtuUpdated(struct bt_conn *conn, uint16_t tx, uint16_t rx)
{
	LOG_DBG("Updated MTU: TX: %d RX: %d bytes", tx, rx);
}

/**
 * @brief Initialize Bluetooth controller and setup callbacks.
 *
 * @param base_uuid Base UUID used for discovery.
 * @param pkt_handler PacketHandler instance to handle data transport / handling.
 *
 * @return CS_OK if the initialization was successful.
 */
cs_ret_code_t BleCentral::init(const char *base_uuid, PacketHandler *pkt_handler)
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	int ret;
	// synchronously enable bluetooth controller
	ret = bt_enable(NULL);
	if (ret) {
		LOG_ERR("Bluetooth init failed (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_BLUETOOTH_INIT_FAILED;
	}

	// the base UUID used for discovery
	if (base_uuid != NULL) {
		_uuid_base.fromFullUuid(base_uuid);
	}
	// CCC UUID for descriptor discovery
	_uuid_ccc.fromShortUuid(BT_UUID_GATT_CCC_VAL);

	memset(&_conn_create_params, 0, sizeof(_conn_create_params));
	_conn_create_params.options = BT_CONN_LE_OPT_NONE;
	// scan continuously by setting scan interval equal to scan window
	_conn_create_params.interval = BT_GAP_SCAN_FAST_INTERVAL;
	_conn_create_params.window = BT_GAP_SCAN_FAST_WINDOW;

	memset(&_conn_init_params, 0, sizeof(_conn_init_params));
	_conn_init_params.interval_max = BT_GAP_INIT_CONN_INT_MAX;
	_conn_init_params.interval_min = BT_GAP_INIT_CONN_INT_MIN;
	_conn_init_params.timeout = CS_BLE_CENTRAL_CONN_TIMEOUT; // 4 s

	memset(&_conn_cbs, 0, sizeof(_conn_cbs));
	_conn_cbs.connected = handleConnectionResult;
	_conn_cbs.disconnected = handleDisonnectionResult;

	memset(&_gatt_cbs, 0, sizeof(_gatt_cbs));
	_gatt_cbs.att_mtu_updated = handleMtuUpdated;

	bt_conn_cb_register(&_conn_cbs);
	bt_gatt_cb_register(&_gatt_cbs);

	k_event_init(&_ble_conn_evts);
	// indicate that we are ready for a connection
	k_event_post(&_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);

	_pkt_handler = pkt_handler;
	_initialized = true;

	return CS_OK;
}

/**
 * @brief Connect to a device with given MAC address.
 * A BLE scan is initiatiated to scan for the device first.
 *
 * @param device_addr Device MAC address in string representation.
 *
 * @return CS_OK if the scan was started successfully.
 */
cs_ret_code_t BleCentral::connect(const char *device_addr)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (isConnected()) {
		LOG_ERR("%s", "Already connected");
		return CS_ERR_BLE_CENTRAL_ALREADY_CONNECTED;
	}

	// convert given string MAC address to bytes
	bt_addr_le_from_str(device_addr, CS_BLE_CENTRAL_ADDR_TYPE_RANDOM_STR, &_dev_addr);
	// add to filter accept list to avoid unnessecary scan results
	bt_le_filter_accept_list_add(&_dev_addr);

	memset(&_scan_params, 0, sizeof(_scan_params));
	_scan_params.type = BT_LE_SCAN_TYPE_PASSIVE;
	_scan_params.options = BT_LE_SCAN_OPT_FILTER_ACCEPT_LIST;
	_scan_params.interval = BT_GAP_SCAN_FAST_INTERVAL;
	_scan_params.window = BT_GAP_SCAN_FAST_WINDOW;

	int ret = bt_le_scan_start(&_scan_params, handleBleDeviceFound);
	if (ret) {
		LOG_ERR("Failed to start BLE scan (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_SCAN_START_FAILED;
	}

	LOG_DBG("%s", "Started BLE scan");

	return CS_OK;
}

/**
 * @brief Discover Gatt services.
 *
 * @param uuids Specific UUID to look for in discovery, so it's results are filtered.
 * If NULL is provided, all services are discovered.
 *
 * @return CS_OK if the discovery was started succesfully.
 */
cs_ret_code_t BleCentral::discoverServices(ServiceUuid *uuid)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	memset(&_gatt_discover_params, 0, sizeof(_gatt_discover_params));
	memset(&_gatt_subscribe_params, 0, sizeof(_gatt_subscribe_params));
	// the union type cs_ble_uuid can contain either bt_uuid_16 or bt_uuid_128
	// which one it is, depends on the type in bt_uuid, which both structs contain at the same
	// location, so it doesn't matter which element we use here
	_gatt_discover_params.uuid = uuid != NULL ? &uuid->getUuid()->uuid_16.uuid : NULL;
	_gatt_discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	_gatt_discover_params.end_handle = UINT8_MAX;
	// discover only primary services to begin with
	_gatt_discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	_gatt_discover_params.func = handleDiscoveryResults;

	int ret = bt_gatt_discover(_conn, &_gatt_discover_params);
	if (ret) {
		LOG_ERR("Failed to start GATT discovery (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_DISCOVERY_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Write a GATT message.
 *
 * @param handle Attribute handle.
 * @param data Data buffer to write.
 * @param len Length of the buffer.
 *
 * @return CS_OK if the write was performed succesfully.
 */
cs_ret_code_t BleCentral::write(uint16_t handle, uint8_t *data, uint16_t len)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	uint16_t mtu = bt_gatt_get_mtu(_conn);
	if (mtu <= CS_BLE_CENTRAL_GATT_WRITE_OVERHEAD) {
		LOG_ERR("%s", "Incorrect MTU, did MTU transfer fail?");
		return CS_ERR_BLE_CENTRAL_INCORRECT_MTU;
	}

	memcpy(_ble_buf, data, len);

	memset(&_gatt_write_params, 0, sizeof(_gatt_write_params));
	_gatt_write_params.data = _ble_buf;
	_gatt_write_params.func = handleWriteResult;
	_gatt_write_params.handle = handle;
	_gatt_write_params.length = len;

	// this function also handles chunked writes in case of len > MTU
	int ret = bt_gatt_write(_conn, &_gatt_write_params);
	if (ret) {
		LOG_ERR("Failed to execute GATT write (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_WRITE_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Read data from characteristics handle.
 *
 * @param handle Characteristics handle.
 *
 * @return CS_OK if the read was performed successfully.
 */
cs_ret_code_t BleCentral::read(uint16_t handle)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	// used to do chunked reads in handler
	_ble_buf_ctr = 0;

	memset(&_gatt_read_params, 0, sizeof(_gatt_read_params));
	_gatt_read_params.func = handleReadResult;
	_gatt_read_params.handle_count = 1;
	_gatt_read_params.single.handle = handle;
	_gatt_read_params.single.offset = 0;

	int ret = bt_gatt_read(_conn, &_gatt_read_params);
	if (ret) {
		LOG_ERR("Failed to execute GATT read (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_READ_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Send a BLE message. A connection will be established first.
 * The device will respond with session data directly after the connection.
 * Callback function for PacketHandler.
 *
 * @param inst Pointer to BleCentral class instance.
 * @param message Pointer to buffer with the message.
 * @param len Length of the message.
 */
void BleCentral::sendBleMessage(k_work *work)
{
	cs_packet_handler *hdlr = CONTAINER_OF(work, cs_packet_handler, work_item);
	BleCentral *ble_inst = BleCentral::getInstance();
	k_spinlock_key_t key;

	key = k_spin_lock(&hdlr->work_lock);

	if (!ble_inst->isConnected()) {
		ble_inst->connect((char *)hdlr->msg.buf);
		k_spin_unlock(&hdlr->work_lock, key);
		return;
	} else {
		if (ble_inst->_controlHandle != 0) {
			ble_inst->write(ble_inst->_controlHandle, hdlr->msg.buf, hdlr->msg.buf_len);
			k_spin_unlock(&hdlr->work_lock, key);
		}
	}
}

/**
 * @brief Wait till a new connection can be established.
 *
 * @param timeout_ms Timeout to wait before a new connection can be made.
 * a timeout of SYS_FOREVER_MS means to wait forever. This will yield control.
 *
 * @return CS_OK if the instance is available for a new connection.
 */
cs_ret_code_t BleCentral::waitAvailable(int timeout_ms)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_timeout_t tout = K_FOREVER;
	if (timeout_ms != SYS_FOREVER_MS) {
		tout = K_MSEC(timeout_ms);
	}

	if (k_event_wait(&_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT, false, tout) == 0) {
		LOG_ERR("%s", "Timeout on waiting for BLE connection to be available");
		return CS_ERR_TIMEOUT;
	}

	return CS_OK;
}

/**
 * @brief Disconnect active connection, or cancel pending connection.
 *
 * @return CS_OK if the disconnection was successful.
 */
cs_ret_code_t BleCentral::disconnect()
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (isConnected()) {
		bt_conn_disconnect(_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
	} else {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	return CS_OK;
}

/**
 * @brief Check whether this module is initialized.
 */
bool BleCentral::isInitialized()
{
	return _initialized;
}

/**
 * @brief Check whether this device is currently connected as central.
 */
bool BleCentral::isConnected()
{
	return _conn != NULL;
}