/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 19 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"

#include <zephyr/bluetooth/uuid.h>

#include <stdint.h>

#define UUID_16_BASE_OFFSET 12

union cs_ble_uuid {
	bt_uuid_16 uuid_16;
	bt_uuid_128 uuid_128;
};

class ServiceUuid
{
      public:
	ServiceUuid() = default;
	/**
	 * @brief Constructor for creating a ServiceUuid instance from a @ref cs_ble_uuid object.
	 */
	ServiceUuid(cs_ble_uuid uuid) : _uuid(uuid){};
	ServiceUuid(const bt_uuid *uuid);

	cs_ret_code_t fromFullUuid(const char *fullUuid);
	cs_ret_code_t fromFullUuid(bt_uuid_128 *fullUuid);
	cs_ret_code_t fromShortUuid(uint16_t shortUuid);
	cs_ret_code_t fromBaseUuid(ServiceUuid *baseUuid, uint16_t shortUuid);

	cs_ble_uuid getUuid() const;

	bool operator==(ServiceUuid other);

      private:
	cs_ble_uuid _uuid;
};