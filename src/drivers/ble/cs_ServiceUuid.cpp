/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 19 Jan., 2023
 * License: Apache License 2.0
 */

#include "drivers/ble/cs_ServiceUuid.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_ServiceUuid, LOG_LEVEL_ERR);

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>

#include <string.h>

/**
 * @brief Create ServiceUuid object from bt_uuid pointer.
 */
ServiceUuid::ServiceUuid(const bt_uuid *uuid)
{
	switch (uuid->type) {
	case BT_UUID_TYPE_16:
		_uuid.uuid_16 = *BT_UUID_16(uuid);
	case BT_UUID_TYPE_128:
		_uuid.uuid_128 = *BT_UUID_128(uuid);
	}
}

/**
 * @brief Load a full UUID (128bit) in string representation.
 *  The UUID should be provided without dashes.
 *
 * @return CS_OK if the operation completed successfully.
 */
cs_ret_code_t ServiceUuid::fromFullUuid(const char *fullUuid)
{
	if (strchr(fullUuid, '-') != NULL) {
		LOG_ERR("%s", "UUID strings should be provided without dashes");
		return CS_ERR_INVALID_PARAM;
	}

	uint8_t uuid128_buf[BT_UUID_SIZE_128];
	int ret = hex2bin(fullUuid, strlen(fullUuid), uuid128_buf, sizeof(uuid128_buf));
	if (ret != sizeof(uuid128_buf)) {
		LOG_ERR("Incorrect UUID provided. %d bytes were expected, but only %d were "
			"converted",
			sizeof(uuid128_buf), ret);
		return CS_ERR_INVALID_PARAM;
	}

	sys_mem_swap(uuid128_buf, sizeof(uuid128_buf));

	if (!bt_uuid_create(&_uuid.uuid_128.uuid, uuid128_buf, sizeof(uuid128_buf))) {
		LOG_ERR("%s", "Failed to create uuid struct from full uuid");
		return CS_ERR_INVALID_PARAM;
	}

	return CS_OK;
}

/**
 * @brief Load a full UUID (128bit) from a @ref bt_uuid_128 structure.
 *
 * @return CS_OK if the operation completed successfully.
 */
cs_ret_code_t ServiceUuid::fromFullUuid(bt_uuid_128 *fullUuid)
{
	_uuid.uuid_128 = *fullUuid;
	return CS_OK;
}

/**
 * @brief Load a short UUID (uint16).
 *
 * @return CS_OK if the operation completed successfully.
 */
cs_ret_code_t ServiceUuid::fromShortUuid(uint16_t shortUuid)
{
	uint8_t uuid16_buf[BT_UUID_SIZE_16];
	sys_put_le16(shortUuid, uuid16_buf);

	if (!bt_uuid_create(&_uuid.uuid_16.uuid, uuid16_buf, sizeof(uuid16_buf))) {
		LOG_ERR("%s", "Failed to create uuid struct from short uuid");
		return CS_ERR_INVALID_PARAM;
	}

	return CS_OK;
}

/**
 * @brief Replace short UUID inside a base UUID. The (128bit) base UUID has to be registered first.
 * This replaces bytes 12 and 13 of the base UUID.
 *
 * @return CS_OK if the operation completed successfully.
 */
cs_ret_code_t ServiceUuid::fromBaseUuid(ServiceUuid *baseUuid, uint16_t shortUuid)
{
	fromFullUuid(&baseUuid->_uuid.uuid_128);
	// our full UUID is reversed, so instead of byte 12 we put it at byte 2, reversed order
	sys_put_le16(shortUuid, &_uuid.uuid_128.val[UUID_16_BASE_OFFSET]);

	return CS_OK;
}

/**
 * @brief Get UUID. Can be either bt_uuid_16 or bt_uuid_128.
 */
cs_ble_uuid *ServiceUuid::getUuid()
{
	return &_uuid;
}

/**
 * @brief Compare ServiceUuid instances by == operator.
 */
bool ServiceUuid::operator==(ServiceUuid other)
{
	return bt_uuid_cmp(&_uuid.uuid_128.uuid, &other._uuid.uuid_128.uuid) ? 0 : 1;
}