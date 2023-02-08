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

#define CS_PACKET_BUF_SIZE   250
#define CS_PACKET_QUEUE_SIZE 14
#define CS_PACKET_HANDLERS   7

#define CS_PACKET_UART_START_TOKEN 0x7E
#define CS_PACKET_UART_CRC_SEED	   0xFFFF

#define CS_PACKET_THREAD_PRIORITY   K_PRIO_COOP(7)
#define CS_PACKET_THREAD_STACK_SIZE 4096

typedef void (*cs_packet_transport_cb_t)(void *inst, uint8_t *msg, int msg_len);

enum cs_packet_transport_type : uint8_t {
	CS_DATA_INCOMING,
	CS_DATA_OUTGOING
};

struct cs_packet_buffer {
	uint8_t buf[CS_PACKET_BUF_SIZE];
	uint16_t buf_len;
};

struct cs_packet_data {
	cs_packet_transport_type type;
	cs_router_instance_id dest_id;
	cs_router_instance_id src_id;
	cs_router_result_code result_code;
	cs_packet_buffer msg;
};

struct cs_packet_result {
	cs_router_command_type type;
	uint16_t id;
};

struct cs_packet_handler {
	k_work work_item;
	k_spinlock work_lock;
	cs_router_instance_id id;
	void *target_inst;
	cs_packet_buffer msg;
	cs_packet_result result;
};

class PacketHandler
{
      public:
	cs_ret_code_t init();
	cs_ret_code_t registerHandler(cs_router_instance_id inst_id, void *inst,
				      k_work_handler_t cb);
	cs_ret_code_t unregisterHandler(cs_router_instance_id inst_id);
	cs_packet_handler *getHandler(cs_router_instance_id inst_id);
	cs_ret_code_t handlePacket(cs_packet_data *data);

	/** Packet message queue */
	k_msgq _pkth_msgq;
	/** Message queue buffer */
	char __aligned(8) _msgq_buf[sizeof(cs_packet_data) * CS_PACKET_QUEUE_SIZE];

	/** Packet handler thread structure instance */
	k_thread _pkth_tid;
	/** Mutex to protect the handlers when the registers is updated */
	k_mutex _pkth_mtx;

      private:
	/** Initialized flag */
	bool _initialized = false;

	/** Packet handler register */
	cs_packet_handler _handlers[CS_PACKET_HANDLERS];
	/** Amount of handlers registered */
	int _handler_ctr = 0;
};