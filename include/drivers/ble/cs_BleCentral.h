/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"

#include <zephyr/bluetooth/conn.h>

#define MAC_ADDRESS_LEN 6

typedef struct __packed {
  uint8_t address[MAC_ADDRESS_LEN];
} cs_device_addr_t;

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
	cs_ret_code_t connect();
	cs_ret_code_t disconnect();

	static void writeGattMessage(void *inst, uint8_t *msg, int msg_len);

	/** BT connection instance */
	bt_conn *_conn;

  /** BT connection callbacks */
  bt_conn_cb _conn_cbs;

      private:
	BleCentral() = default;
};