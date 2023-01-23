/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 19 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#define CROWNSTONE_UUID "24f000007d104805bfc17663a01c3bff"

enum cs_ble_central_characteristics_ids {
	FACTORY_RESET_UUID = 0x9,
	CONTROL_UUID = 0xC,
	RESULT_UUID = 0xD,
	SESSION_DATA_UUID = 0xE,
	SESSION_DATA_UNENCRYPTED_UUID = 0xF,
};

class CrownstoneCentral
{
      private:
	/** Handle used to read the session key */
	uint16_t _sessionKeyHandle;
	/** Handle used to read session data */
	uint16_t _sessionDataHandle;
	/** Handle used to control the device */
	uint16_t _controlHandle;
	/** Handle used to retrieve a result from the device */
	uint16_t _resultHandle;
	/** Handle used to read characteristics */
	uint16_t _resultCccdHandle;
};