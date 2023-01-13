/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 13 Jan., 2023
 * License: Apache License 2.0
 */

#pragma once

#include "cs_Router.h"

#include <stdint.h>

#define CS_PACKET_UART_START_TOKEN 0x7E
#define CS_PACKET_UART_CRC_SEED	   0xFFFF

int wrapUartPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf);
int wrapGenericPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf);
int wrapDataPacket(uint8_t src_type, uint8_t src_id, uint8_t *payload, int payload_len,
		   uint8_t *pkt_buf);

void loadUartPacket(cs_router_uart_packet *uart_pkt, uint8_t *buffer);
void loadGenericPacket(cs_router_generic_packet *generic_pkt, uint8_t *buffer);
void loadControlPacket(cs_router_control_packet *ctrl_pkt, uint8_t *buffer);
void loadSwitchCommandPacket(cs_router_switch_command_packet *switch_pkt, uint8_t *buffer);

void handleOutgoingPacket(uint8_t *packet, int packet_len, uint8_t dest_type, cs_router_instances *inst);
void handleIncomingPacket(uint8_t *packet);