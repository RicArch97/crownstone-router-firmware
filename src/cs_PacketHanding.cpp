/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 13 Jan., 2023
 * License: Apache License 2.0
 */

#include "cs_PacketHandling.h"
#include "drivers/cs_Uart.h"
#include "socket/cs_WebSocket.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_PacketHandling, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

/**
 * @brief Wrap a payload into an UART packet.
 *
 * @param type One of @ref cs_router_uart_packet_type, type of the payload
 * @param payload Buffer that should be wrapped into an UART packet
 * @param payload_len Length of the buffer in bytes
 * @param pkt_buf Buffer where the packet should be stored
 *
 * @return Length of the UART packet in bytes.
 */
int wrapUartPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
{
	int uart_pkt_ctr = 0;
	int uart_pkt_len = (sizeof(cs_router_uart_packet) - sizeof(uint8_t *)) + payload_len;

	pkt_buf[uart_pkt_ctr++] = CS_PACKET_UART_START_TOKEN;
	sys_put_be16(uart_pkt_len - 3, pkt_buf + uart_pkt_ctr);
	uart_pkt_ctr += 2;
	pkt_buf[uart_pkt_ctr++] = CS_UART_PROTOCOL_VERSION;
	pkt_buf[uart_pkt_ctr++] = type;
	memcpy(pkt_buf + uart_pkt_ctr, payload, payload_len);
	uart_pkt_ctr += payload_len;

	// calculate CRC16 CCITT over everything after length (so
	// don't include start token and length)
	uint16_t crc = crc16_ccitt(CS_PACKET_UART_CRC_SEED, pkt_buf + 3, uart_pkt_ctr - 3);
	sys_put_be16(crc, pkt_buf + uart_pkt_ctr);

	return uart_pkt_len;
}

/**
 * @brief Wrap a payload into a generic packet.
 *
 * @param type One of @ref cs_router_generic_packet_type, type of the payload
 * @param payload Buffer that should be wrapped into a generic packet
 * @param payload_len Length of the buffer in bytes
 * @param pkt_buf Buffer where the packet should be stored
 *
 * @return Length of the generic packet in bytes.
 */
int wrapGenericPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
{
	pkt_buf[0] = CS_PROTOCOL_VERSION;
	pkt_buf[1] = type;
	sys_put_be16(payload_len, pkt_buf + 2);
	memcpy(pkt_buf + 4, payload, payload_len);

	return (sizeof(cs_router_generic_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Wrap a payload into a data packet.
 *
 * @param src_type One of @ref cs_router_instance_type, type of the payload source
 * @param src_id One of @ref cs_router_instance_uart_id, @ref cs_router_instance_cloud_id or
 * @ref cs_router_instance_ble_id. Identifier of the payload source
 * @param payload Buffer that should be wrapped into a data packet
 * @param payload_len Length of the buffer in bytes
 * @param pkt_buf Buffer where the packet should be stored
 *
 * @return Length of the data packet in bytes.
 */
int wrapDataPacket(uint8_t src_type, uint8_t src_id, uint8_t *payload, int payload_len,
		   uint8_t *pkt_buf)
{
	pkt_buf[0] = src_type;
	pkt_buf[1] = src_id;
	sys_put_be16(payload_len, pkt_buf + 2);
	memcpy(pkt_buf + 4, payload, payload_len);

	return (sizeof(cs_router_data_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Load a UART packet from a buffer.
 *
 * @param pkt Pointer to instance of @ref cs_router_uart_packet, which should be loaded with data
 * @param buffer Buffer with data that should be created into an UART packet
 */
void loadUartPacket(cs_router_uart_packet *uart_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	uart_pkt->start_token = buffer[pkt_ctr++];
	uart_pkt->length = sys_get_be16(buffer + pkt_ctr);
	pkt_ctr += 2;
	uart_pkt->protocol_version = buffer[pkt_ctr++];
	uart_pkt->type = buffer[pkt_ctr++];

	int payload_len = uart_pkt->length - 4; // minus protocol ver, type and CRC
	uart_pkt->payload = (uint8_t *)k_malloc(payload_len * sizeof(uint8_t));
	if (uart_pkt->payload == NULL) {
		LOG_ERR("Failed to allocate memory for UART packet payload");
		return;
	}
	memcpy(uart_pkt->payload, buffer + pkt_ctr, payload_len);
	pkt_ctr += payload_len;

	// check CRC CCITT over everything after length, not including CRC (uint16) itself
	uint16_t check_crc = crc16_ccitt(CS_PACKET_UART_CRC_SEED, buffer + 3, uart_pkt->length - 2);
	uint16_t received_crc = sys_get_be16(buffer + pkt_ctr);
	// CRC doesn't match, invalid packet
	if (check_crc != received_crc) {
		LOG_WRN("CRC mismatch on received UART packet. Calculated: %hu, Received: %hu",
			check_crc, received_crc);
	}
}

/**
 * @brief Load a generic packet from a buffer.
 *
 * @param generic_pkt Pointer to instance of @ref cs_router_generic_packet, which should be loaded
 * with data
 * @param buffer Buffer with data that should be created into a generic packet
 */
void loadGenericPacket(cs_router_generic_packet *generic_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	generic_pkt->protocol_version = buffer[pkt_ctr++];
	generic_pkt->type = buffer[pkt_ctr++];
	generic_pkt->length = sys_get_be16(buffer + pkt_ctr);
	pkt_ctr += 2;
	generic_pkt->payload = (uint8_t *)k_malloc(generic_pkt->length * sizeof(uint8_t));
	if (generic_pkt->payload == NULL) {
		LOG_ERR("Failed to allocate memory for generic packet payload");
		return;
	}
	memcpy(generic_pkt->payload, buffer + pkt_ctr, generic_pkt->length);

	if (generic_pkt->protocol_version != CS_PROTOCOL_VERSION) {
		LOG_WRN("Protocol mismatch: firmware protocol: %d, provided protocol: %d",
			CS_PROTOCOL_VERSION, generic_pkt->protocol_version);
	}
}

/**
 * @brief Load a control packet from a buffer.
 *
 * @param ctrl_pkt Pointer to instance of @ref cs_router_control_packet, which should be loaded with
 * data
 * @param buffer Buffer with data that should be created into a control packet
 */
void loadControlPacket(cs_router_control_packet *ctrl_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	ctrl_pkt->command_type = buffer[pkt_ctr++];
  ctrl_pkt->dest_type = buffer[pkt_ctr++];
	ctrl_pkt->dest_id = buffer[pkt_ctr++];
	ctrl_pkt->length = sys_get_be16(buffer + pkt_ctr);
	pkt_ctr += 2;
	ctrl_pkt->payload = (uint8_t *)k_malloc(ctrl_pkt->length * sizeof(uint8_t));
	if (ctrl_pkt->payload == NULL) {
		LOG_ERR("Failed to allocate memory for control packet payload");
		return;
	}
	memcpy(ctrl_pkt->payload, buffer + pkt_ctr, ctrl_pkt->length);
}

/**
 * @brief Load a switch command packet from a buffer.
 *
 * @param switch_pkt Pointer to instance of @ref cs_router_switch_command_packet, which should be
 * loaded with data
 * @param buffer Buffer with data that should be create into a switch command packet
 */
void loadSwitchCommandPacket(cs_router_switch_command_packet *switch_pkt, uint8_t *buffer)
{
	switch_pkt->switch_value = buffer[0];
}

/**
 * @brief Handle an outgoing packet by routing it to the given destination.
 *
 * @param packet Packet buffer generated by one of the wrapper functions
 * @param packet_len Length of the packet in bytes
 * @param dest_type Destination type of the packet, where it should be routed to
 * @param cls The class instance of the packet destination, according to the dest_id.
 * This will be cast into the class type matching the dest_id.
 */
void handleOutgoingPacket(uint8_t *packet, int packet_len, uint8_t dest_type, cs_router_instances *inst)
{
	switch (dest_type) {
  case CS_INSTANCE_ID_ESP32:
    LOG_HEXDUMP_DBG(packet, (uint32_t)packet_len, "packet");
	case CS_INSTANCE_TYPE_UART:
		inst->rs485->sendUartMessage(packet, packet_len);
		break;
	case CS_INSTANCE_TYPE_CLOUD:
		inst->ws->sendMessage(packet, packet_len);
		break;
	case CS_INSTANCE_TYPE_BLE:
		break;
	default:
		break;
	}
}