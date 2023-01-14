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

struct cs_packet_handler {
	cs_router_instance_id id;
	void *cls;
	void (*handler)(void *, uint8_t *, int);
};

class PacketHandler
{
public:
	PacketHandler() = default;

	void registerTransportHandler(cs_router_instance_id inst_id, void *cls,
				      void (*handle_func)(void *, uint8_t *, int));
	void unregisterTransportHandler(cs_router_instance_id inst_id);
	void handleIncomingPacket(uint8_t *buffer, bool is_uart_pkt);
	void handlePeripheralData(cs_router_instance_id src_id, cs_router_instance_id dest_id,
				  uint8_t *buffer, int buffer_len);

private:
	void transportPacket(cs_router_instance_id inst_id, uint8_t *buffer, int buffer_len);

	cs_packet_handler _handlers[CS_INSTANCE_ID_AMOUNT];
	int _handler_ctr = 0;
};