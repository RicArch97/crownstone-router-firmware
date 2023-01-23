/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 13 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_Router.h"
#include "cs_ReturnTypes.h"

#include <zephyr/kernel.h>

#include <stdint.h>
#include <stdbool.h>

#define CS_PACKET_BUF_SIZE 256

#define CS_PACKET_UART_START_TOKEN 0x7E
#define CS_PACKET_UART_CRC_SEED	   0xFFFF

typedef void (*cs_packet_transport_cb_t)(void *inst, uint8_t *msg, int msg_len);

struct cs_packet_handler {
	cs_router_instance_id id;
	void *inst;
	cs_packet_transport_cb_t cb;
};

class PacketHandler
{
      public:
	void init();
	void registerTransportHandler(cs_router_instance_id inst_id, void *inst,
				      cs_packet_transport_cb_t cb);
	void unregisterTransportHandler(cs_router_instance_id inst_id);
	void handleIncomingPacket(uint8_t *buffer, bool is_uart_pkt);
	void handlePeripheralData(cs_router_instance_id src_id, cs_router_instance_id dest_id,
				  uint8_t *buffer, int buffer_len);

      private:
	void transportPacket(cs_router_instance_id inst_id, uint8_t *buffer, int buffer_len);

	int wrapUartPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf);
	int wrapGenericPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf);
	int wrapDataPacket(uint8_t src_id, uint8_t *payload, int payload_len, uint8_t *pkt_buf);

	void loadUartPacket(cs_router_uart_packet *uart_pkt, uint8_t *buffer);
	void loadGenericPacket(cs_router_generic_packet *generic_pkt, uint8_t *buffer);
	void loadControlPacket(cs_router_control_packet *ctrl_pkt, uint8_t *buffer);
	void loadSwitchCommandPacket(cs_router_switch_command_packet *switch_pkt, uint8_t *buffer);

	/** Register of callbacks for handling message buffer for instances */
	cs_packet_handler _handlers[CS_INSTANCE_ID_AMOUNT];

	/** Mutex to protect handlers */
	k_mutex _pkth_mtx;

	/** Amount of callbacks currently registered */
	int _handler_ctr = 0;
};