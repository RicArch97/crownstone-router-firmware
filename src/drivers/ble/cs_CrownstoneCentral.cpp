/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 19 Jan., 2023
 * License: Apache License 2.0
 */

#include "drivers/ble/cs_CrownstoneCentral.h"
#include "drivers/ble/cs_BleCentral.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_CrownstoneCentral, LOG_LEVEL_DBG);

K_THREAD_STACK_DEFINE(crwn_central_tid_stack_area, CS_CRWN_CENTRAL_THREAD_STACK_SIZE);

/**
 * @brief Handle Crownstone write result messages in the message queue.
 *
 * @param inst Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
static void handleResultMessages(void *inst, void *unused1, void *unused2)
{
  return;
}

/**
 * @brief Handle Crownstone discovery results.
 */
static uint8_t handleCrownstoneDiscoveryResults(bt_conn *conn, const bt_gatt_attr *attr,
						bt_gatt_discover_params *params)
{
	CrownstoneCentral *crwn_inst = &CrownstoneCentral::getInstance();
	// there are no more results left
	if (!attr) {
		LOG_INF("%s", "Crownstone discovery completed.");
		return BT_GATT_ITER_STOP;
	}

	if (params->type == BT_GATT_DISCOVER_PRIMARY) {
		bt_gatt_service_val *srv_val = static_cast<bt_gatt_service_val *>(attr->user_data);

		ServiceUuid found_uuid(srv_val->uuid);
		if (found_uuid == crwn_inst->_crwn_uuid_full) {
			LOG_DBG("%s", "Discovered primary Crownstone service");
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
		LOG_ERR("Failed to write %hu bytes to Crownstone %s with handle %hu (err %d)",
			params->length, dev, params->handle, err);
		return;
	}

	LOG_DBG("Wrote %hu bytes to Crownstone %s with handle %hu", params->length, dev,
		params->handle);
}

cs_ret_code_t CrownstoneCentral::init()
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	cs_ret_code_t ret;
	if (!BleCentral::getInstance().isInitialized()) {
		ret = BleCentral::getInstance().init();
		if (ret) {
			return ret;
		}
	}

	// the Crownstone base UUID
	_crwn_uuid_full.fromFullUuid(CROWNSTONE_UUID);

	k_timer_init(&_conn_timer, NULL, NULL);
	// initialize message queue, aligned to 4-byte boundary
	k_msgq_init(&_crwn_central_msgq, _msgq_buf, CS_CRWN_CENTRAL_BUFFER_SIZE,
		    CS_CRWN_CENTRAL_QUEUE_SIZE);

	// create thread for handling uart messages and packets
	k_tid_t uart_thread = k_thread_create(&_crwn_central_tid, crwn_central_tid_stack_area,
					      K_THREAD_STACK_SIZEOF(crwn_central_tid_stack_area),
					      handleResultMessages, this, NULL, NULL,
					      CS_CRWN_CENTRAL_THREAD_PRIORITY, 0, K_NO_WAIT);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Connect to a Crownstone by a given MAC address.
 *
 * @param device_addr The MAC address of the Crownstone.
 * @param timeout_ms The amoumt of milliseconds to wait before giving up.
 */
cs_ret_code_t CrownstoneCentral::connect(const char *device_addr, uint16_t timeout_ms)
{
	cs_ret_code_t ret;

	k_timer_start(&_conn_timer, K_MSEC(timeout_ms), K_NO_WAIT);

	// we only keep 1 connection reference, in case a device is already connected
	// we must wait until the reference becomes available again
	if (BleCentral::getInstance().isConnected()) {
		ret = BleCentral::getInstance().waitAvailable(timeout_ms);
		if (ret) {
			return ret;
		}
	}

	uint32_t remaining_time = k_timer_remaining_get(&_conn_timer);
	k_timer_stop(&_conn_timer);
	if (remaining_time == 0U) {
		LOG_ERR("%s", "Timeout on waiting for Crownstone connection");
		return CS_ERR_TIMEOUT;
	}

	ret = BleCentral::getInstance().connect(device_addr);
	if (ret) {
		return ret;
	}

	// wait till we're connected to start discovery. this is an async wait
	ret = BleCentral::getInstance().waitConnected(SYS_FOREVER_MS);
	if (ret) {
		LOG_ERR("Failed to connect to Crownstone: %s", device_addr);
		return CS_ERR_BLE_CENTRAL_CONNECTION_FAILED;
	}

	// start discovery. Check for the specific Crownstone primary UUID only
	return BleCentral::getInstance().discoverServices(&_crwn_uuid_full,
							  handleCrownstoneDiscoveryResults);
}

/**
 * @brief Manually terminate connection with a Crownstone.
 *
 * @return CS_OK if the disconnection was successful.
 */
cs_ret_code_t CrownstoneCentral::disconnect()
{
	return BleCentral::getInstance().disconnect();
}