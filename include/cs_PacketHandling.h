/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 13 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_RouterProtocol.h"
#include "cs_ReturnTypes.h"

#include <zephyr/kernel.h>

#include <stdint.h>
#include <stdbool.h>

#define CS_PACKET_HANDLERS_INPUT  2
#define CS_PACKET_HANDLERS_OUTPUT 7

#define CS_PACKET_BUF_SIZE 256

#define CS_PACKET_UART_START_TOKEN 0x7E
#define CS_PACKET_UART_CRC_SEED	   0xFFFF

typedef void (*cs_packet_transport_cb_t)(void *inst, uint8_t *msg, int msg_len);

enum cs_packet_transport_type {
	CS_DATA_INCOMING,
	CS_DATA_OUTGOING
};

struct cs_packet_data {
	cs_packet_transport_type type;
	cs_router_instance_id dest_id;
	cs_router_instance_id src_id;
	cs_router_result_code result_code;
	uint8_t *buffer;
	uint16_t buffer_len;
};

struct cs_packet_handler_input {
	k_work work_item;
	k_sem work_sem;
	cs_router_instance_id source_id;
	uint8_t buffer[CS_PACKET_BUF_SIZE];
	void *ph_inst;
};

struct cs_packet_handler_output {
	k_work work_item;
	k_sem work_sem;
	cs_router_instance_id target_id;
	cs_router_instance_id source_id;
	cs_router_result_code result_code;
	void *target_inst;
	cs_packet_transport_cb_t cb;
	uint8_t buffer[CS_PACKET_BUF_SIZE];
	uint16_t buffer_len;
	uint16_t result_id;
};

class PacketHandler
{
      public:
	cs_ret_code_t init();
	cs_ret_code_t registerInputHandler(cs_router_instance_id inst_id);
	cs_ret_code_t registerOutputHandler(cs_router_instance_id inst_id, void *inst,
					    cs_packet_transport_cb_t cb);
	cs_ret_code_t unregisterHandler(cs_packet_transport_type type,
					cs_router_instance_id inst_id);
	cs_packet_handler_output *getOutputHandler(cs_router_instance_id inst_id);
	cs_ret_code_t handlePacket(cs_packet_data *data);

      private:
	/** Initialized flag */
	bool _initialized = false;

	/**
	 * Register of handlers for incoming packets
	 * This amount is 2: UART CM4 and websocket
	 */
	cs_packet_handler_input _in_handlers[CS_PACKET_HANDLERS_INPUT];
	/**
	 * Register of handlers for outgoing packets
	 * This amount is 7: UART RS485/RS232/CM4, websocket, BLE mesh/peripheral
	 */
	cs_packet_handler_output _out_handlers[CS_PACKET_HANDLERS_OUTPUT];

	/** Mutexes to protect handlers */
	k_mutex _out_pkth_mtx;
	k_mutex _in_pkth_mtx;

	/** Amount of outgoing handlers currently registered */
	int _in_handler_ctr = 0;
	/** Amount of outgoing handlers currently registered */
	int _out_handler_ctr = 0;
};