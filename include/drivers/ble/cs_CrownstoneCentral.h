/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 19 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"
#include "drivers/ble/cs_ServiceUuid.h"

#include <zephyr/kernel.h>

#include <stdint.h>

#define CROWNSTONE_UUID "24f000007d104805bfc17663a01c3bff"

#define CS_CRWN_CENTRAL_THREAD_PRIORITY	  K_PRIO_PREEMPT(7)
#define CS_CRWN_CENTRAL_THREAD_STACK_SIZE 4096

#define CS_CRWN_CENTRAL_BUFFER_SIZE 256
#define CS_CRWN_CENTRAL_QUEUE_SIZE  3

enum cs_crownstone_central_characteristics_ids {
	CONTROL_UUID = 0xC,
	RESULT_UUID = 0xD,
};

class CrownstoneCentral
{
      public:
	static CrownstoneCentral &getInstance()
	{
		static CrownstoneCentral instance;
		return instance;
	}
	// Deny implementation
	CrownstoneCentral(CrownstoneCentral const &) = delete;
	CrownstoneCentral(CrownstoneCentral &&) = delete;
	void operator=(CrownstoneCentral const &) = delete;
	void operator=(CrownstoneCentral &&) = delete;

	cs_ret_code_t init();
	cs_ret_code_t connect(const char *device_addr, uint16_t timeout_ms = 3000);
	cs_ret_code_t disconnect();

	static void sendBleMessage(void *inst, uint8_t *msg, uint8_t msg_len);

	/** UART thread structure instance */
	k_thread _crwn_central_tid;
	/** Timer to time the connection wait available and scan timeout */
	k_timer _conn_timer;

	/** Crownstone write result data message queue structure instance */
	k_msgq _crwn_central_msgq;
	/** Crownstone write result data buffer used by the message queue */
	char __aligned(4) _msgq_buf[CS_CRWN_CENTRAL_BUFFER_SIZE * CS_CRWN_CENTRAL_QUEUE_SIZE];

	ServiceUuid _crwn_uuid_full;

	/** Handle used to control the device */
	uint16_t _controlHandle;
	/** Handle used to retrieve a result from the device */
	uint16_t _resultHandle;
	/** Handle used to read characteristics */
	uint16_t _resultCccdHandle;

      private:
	CrownstoneCentral() = default;

	/** Initialized flag */
	bool _initialized = false;
};