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

/**
 * @brief Handle MTU exchange result.
 */
static void handleMtuExchangeResult(bt_conn *conn, uint8_t err, bt_gatt_exchange_params *params)
{
	LOG_INF("MTU exchange %s (%u)", err == 0U ? "successful" : "failed", bt_gatt_get_mtu(conn));
}

/**
 * @brief Handle BLE device found.
 */
static void handleBleDeviceFound(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
				 net_buf_simple *ad)
{
	BleCentral *ble_inst = &BleCentral::getInstance();

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
	// we have found the device, stop the scan`
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
		ble_inst->connect(dev);
	}
}

/**
 * @brief Handle connection established to BLE device.
 */
static void handleConnectionResult(bt_conn *conn, uint8_t conn_err)
{
	BleCentral *ble_inst = &BleCentral::getInstance();

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	if (conn_err) {
		LOG_ERR("Failed to connect to %s (%u)", dev, conn_err);

		bt_conn_unref(ble_inst->_conn);
		ble_inst->_conn = NULL;

		ble_inst->connect(dev);
		return;
	}

	// indicate that the instance is now in use / connected
	k_event_clear(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);
	k_event_post(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_CONNECTED_EVENT);

	LOG_INF("Connected: %s", dev);

	ble_inst->_gatt_exchange_params.func = handleMtuExchangeResult;
	int ret = bt_gatt_exchange_mtu(conn, &ble_inst->_gatt_exchange_params);
	if (ret) {
		LOG_ERR("Failed to exchange MTU (err %d", ret);
		return;
	}
}

/**
 * @brief Handle disconnection from BLE device.
 */
static void handleDisonnectionResult(bt_conn *conn, uint8_t reason)
{
	BleCentral *ble_inst = &BleCentral::getInstance();

	char dev[BT_ADDR_LE_STR_LEN];
	bt_addr_le_to_str(bt_conn_get_dst(conn), dev, sizeof(dev));

	LOG_INF("Disconnected from BLE device: %s (reason 0x%02x)", dev, reason);

	if (ble_inst->_conn != conn) {
		return;
	}

	bt_conn_unref(ble_inst->_conn);
	ble_inst->_conn = NULL;
	// we are ready for a new connection again
	k_event_post(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);
	k_event_clear(&ble_inst->_ble_conn_evts, CS_BLE_CENTRAL_CONNECTED_EVENT);

	// if we didn't manually disconnect, retry
	if (reason != BT_HCI_ERR_REMOTE_USER_TERM_CONN) {
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
 * @return CS_OK if the initialization was successful.
 */
cs_ret_code_t BleCentral::init()
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

	k_event_init(&_ble_conn_evts);
	// indicate that we are ready for a connection
	k_event_post(&_ble_conn_evts, CS_BLE_CENTRAL_AVAILABLE_EVENT);

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

	memset(&_scan_params, 0, sizeof(_scan_params));
	_scan_params.type = BT_LE_SCAN_TYPE_ACTIVE;
	_scan_params.options = BT_LE_SCAN_OPT_NONE;
	_scan_params.interval = BT_GAP_SCAN_FAST_INTERVAL;
	_scan_params.window = BT_GAP_SCAN_FAST_WINDOW;

	int ret = bt_le_scan_start(&_scan_params, handleBleDeviceFound);
	if (ret) {
		LOG_ERR("Failed to start BLE scan (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_SCAN_START_FAILED;
	}

	LOG_INF("%s", "Started BLE scan");

	return CS_OK;
}

/**
 * @brief Discover Gatt services.
 *
 * @param uuids Specific UUID to look for in discovery, so it's results are filtered.
 * If NULL is provided, all services are discovered.
 * @param cb Callback to handle discovery results. Handling depends on the device.
 *
 * @return CS_OK if the discovery was started succesfully.
 */
cs_ret_code_t BleCentral::discoverServices(ServiceUuid *uuid, bt_gatt_discover_func_t cb)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	if (uuid != NULL) {
		_discover_uuid = uuid->getUuid();
	}

	memset(&_gatt_discover_params, 0, sizeof(_gatt_discover_params));
	// the union type cs_ble_uuid can contain either bt_uuid_16 or bt_uuid_128
	// which one it is, depends on the type in bt_uuid, which both structs contain at the same
	// location, so it doesn't matter which element we use here
	_gatt_discover_params.uuid = uuid != NULL ? &_discover_uuid.uuid_16.uuid : NULL;
	_gatt_discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
	_gatt_discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
	// discover only primary services to begin with
	_gatt_discover_params.type = BT_GATT_DISCOVER_PRIMARY;
	_gatt_discover_params.func = cb;

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
 * @param cb Callback to handle the write result.
 *
 * @return CS_OK if the write was performed succesfully.
 */
cs_ret_code_t BleCentral::write(uint16_t handle, uint8_t *data, uint16_t len,
				bt_gatt_write_func_t cb)
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

	memcpy(_ble_write_buf, data, len);

	memset(&_gatt_write_params, 0, sizeof(_gatt_write_params));
	_gatt_write_params.data = _ble_write_buf;
	_gatt_write_params.func = cb;
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
 * @param cb Callback to handle the read result.
 *
 * @return CS_OK if the read was performed successfully.
 */
cs_ret_code_t BleCentral::read(uint16_t handle, bt_gatt_read_func_t cb)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	memset(&_gatt_read_params, 0, sizeof(_gatt_read_params));
	_gatt_read_params.func = cb;
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
 * @brief Read data from mutliple characteristics handle.
 * The data returned can be of variable length.
 *
 * @param handles Characteristics handles.
 * @param handleCount Amount of characteristics handles provided.
 * @param cb Callback to handle the read results.
 *
 * @return CS_OK if the read was performed successfully.
 */
cs_ret_code_t BleCentral::read(uint16_t *handles, uint8_t handleCount, bt_gatt_read_func_t cb)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (!isConnected()) {
		LOG_ERR("%s", "Not connected");
		return CS_ERR_BLE_CENTRAL_NOT_CONNECTED;
	}

	memset(&_gatt_read_params, 0, sizeof(_gatt_read_params));
	_gatt_read_params.func = cb;
	_gatt_read_params.handle_count = handleCount;
	_gatt_read_params.multiple.handles = handles;
	_gatt_read_params.multiple.variable = true;

	int ret = bt_gatt_read(_conn, &_gatt_read_params);
	if (ret) {
		LOG_ERR("Failed to execute GATT read (err %d)", ret);
		return CS_ERR_BLE_CENTRAL_READ_FAILED;
	}

	return CS_OK;
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
 * @brief Wait till a new connection is established.
 *
 * @param timeout_ms Timeout to wait before a new connection is established.
 * a timeout of SYS_FOREVER_MS means to wait forever. This will yield control.
 *
 * @return CS_OK if the instance is connected.
 */
cs_ret_code_t BleCentral::waitConnected(int timeout_ms)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_timeout_t tout = K_FOREVER;
	if (timeout_ms != SYS_FOREVER_MS) {
		tout = K_MSEC(timeout_ms);
	}

	if (k_event_wait(&_ble_conn_evts, CS_BLE_CENTRAL_CONNECTED_EVENT, false, tout) == 0) {
		LOG_ERR("%s", "Timeout on waiting for BLE connection");
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