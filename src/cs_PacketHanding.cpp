/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 13 Jan., 2023
 * License: Apache License 2.0
 */

#include "cs_PacketHandling.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_PacketHandling, LOG_LEVEL_INF);

#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/crc.h>

/**
 * @brief Register a handler that can be used for data transport.
 *
 * @param inst_id Instance ID of the instance that handle transport
 * @param inst Pointer to class instance according to instance ID
 * @param cb Function pointer to function that should be called with the instance.
 */
void PacketHandler::registerTransportHandler(cs_router_instance_id inst_id, void *inst,
					     cs_packet_transport_cb_t cb)
{
	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			LOG_ERR("Handler with ID %d already registered", inst_id);
			return;
		}
	}
	// store handler
	_handlers[_handler_ctr++] = {inst_id, inst, cb};
}

/**
 * @brief Unregister a registered transport handler.
 *
 * @param inst_id Instance ID that was registered for handling transport
 */
void PacketHandler::unregisterTransportHandler(cs_router_instance_id inst_id)
{
	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			// clean from register by shifting next data to removed slot
			for (int j = (i - 1); i < (_handler_ctr - 1); i++) {
				_handlers[j] = _handlers[j + 1];
			}
			_handler_ctr--;
			return;
		}
	}
	LOG_ERR("Could not find handler for ID %d", inst_id);
}

/**
 * @brief Handle an incoming packet, from either CM4 or cloud.
 *
 * @param buffer Pointer to the received packet buffer
 * @param is_uart_pkt Whether this is a packet coming from UART (CM4)
 */
void PacketHandler::handleIncomingPacket(uint8_t *buffer, bool is_uart_pkt)
{
	cs_router_generic_packet generic_pkt;

	if (is_uart_pkt) {
		cs_router_uart_packet uart_pkt;
		loadUartPacket(&uart_pkt, buffer);
		loadGenericPacket(&generic_pkt, uart_pkt.payload);
	} else {
		loadGenericPacket(&generic_pkt, buffer);
	}

	if (generic_pkt.type == CS_PACKET_TYPE_CONTROL) {
		cs_router_control_packet ctrl_pkt;
		loadControlPacket(&ctrl_pkt, generic_pkt.payload);
		// currently the only packet that is transported to peripherals
		if (ctrl_pkt.command_type == CS_COMMAND_TYPE_SWITCH) {
			cs_router_switch_command_packet switch_pkt;
			loadSwitchCommandPacket(&switch_pkt, ctrl_pkt.payload);

			transportPacket(static_cast<cs_router_instance_id>(ctrl_pkt.dest_id),
					&switch_pkt.switch_value, ctrl_pkt.length);
		}
	}
	// unhandled: CS_PACKET_TYPE_RESULT: not yet implemented
	// unhandled: CS_PACKET_TYPE_DATA: not sent to this device, only from
}

/**
 * @brief Handle data coming from peripherals. This is always wrapped into a data packet
 * as the contents are unknown and should be handled by the application
 *
 * @param src_id Instance ID of the data source
 * @param dest_id Instance ID of the destination, where the data should be send to
 * @param buffer Pointer to buffer with the data
 * @param buffer_len Length of the buffer
 */
void PacketHandler::handlePeripheralData(cs_router_instance_id src_id,
					 cs_router_instance_id dest_id, uint8_t *buffer,
					 int buffer_len)
{
	uint8_t pkt_buf[CS_PACKET_BUF_SIZE];

	int data_pkt_len = wrapDataPacket(src_id, buffer, buffer_len, pkt_buf);

	uint8_t data_pkt_tmp_buf[data_pkt_len];
	memcpy(data_pkt_tmp_buf, pkt_buf, data_pkt_len);

	int generic_pkt_len =
		wrapGenericPacket(CS_PACKET_TYPE_DATA, data_pkt_tmp_buf, data_pkt_len, pkt_buf);
	int pkt_len = generic_pkt_len;

	// when packet should be routed to CM4
	if (dest_id == CS_INSTANCE_ID_UART_CM4) {
		uint8_t generic_pkt_tmp_buf[generic_pkt_len];
		memcpy(generic_pkt_tmp_buf, pkt_buf, generic_pkt_len);

		pkt_len = wrapUartPacket(CS_PACKET_TYPE_GENERIC, generic_pkt_tmp_buf,
					 generic_pkt_len, pkt_buf);
	}

	// transport functions either copy the data or wait before buffer
	// is processed before reaching the end of function
	transportPacket(dest_id, pkt_buf, pkt_len);
}

/**
 * @brief Transport packet to an instance according to a provided
 * instance id, by calling its registered transport function.
 *
 * @param inst_id Instance ID of the instance that should handle transport
 * @param buffer Buffer with data to be transported
 * @param buffer_len Length of the buffer in bytes
 */
void PacketHandler::transportPacket(cs_router_instance_id inst_id, uint8_t *buffer, int buffer_len)
{
	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			_handlers[i].cb(_handlers[i].inst, buffer, buffer_len);
			return;
		}
	}
	LOG_ERR("Could not find handler for ID %d", inst_id);
}

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
int PacketHandler::wrapUartPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
{
	int uart_pkt_ctr = 0;
	int uart_pkt_len = (sizeof(cs_router_uart_packet) - sizeof(uint8_t *)) + payload_len;

	pkt_buf[uart_pkt_ctr++] = CS_PACKET_UART_START_TOKEN;
	sys_put_le16(uart_pkt_len - 3, pkt_buf + uart_pkt_ctr);
	uart_pkt_ctr += 2;
	pkt_buf[uart_pkt_ctr++] = CS_UART_PROTOCOL_VERSION;
	pkt_buf[uart_pkt_ctr++] = type;
	memcpy(pkt_buf + uart_pkt_ctr, payload, payload_len);
	uart_pkt_ctr += payload_len;

	// calculate CRC16 CCITT over everything after length (so
	// don't include start token and length)
	uint16_t crc = crc16_ccitt(CS_PACKET_UART_CRC_SEED, pkt_buf + 3, uart_pkt_ctr - 3);
	sys_put_le16(crc, pkt_buf + uart_pkt_ctr);

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
int PacketHandler::wrapGenericPacket(uint8_t type, uint8_t *payload, int payload_len,
				     uint8_t *pkt_buf)
{
	pkt_buf[0] = CS_PROTOCOL_VERSION;
	pkt_buf[1] = type;
	sys_put_le16(payload_len, pkt_buf + 2);
	memcpy(pkt_buf + 4, payload, payload_len);

	return (sizeof(cs_router_generic_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Wrap a payload into a data packet.
 *
 * @param src_id One of @ref cs_router_instance_id, identifier of the payload source
 * @param payload Buffer that should be wrapped into a data packet
 * @param payload_len Length of the buffer in bytes
 * @param pkt_buf Buffer where the packet should be stored
 *
 * @return Length of the data packet in bytes.
 */
int PacketHandler::wrapDataPacket(uint8_t src_id, uint8_t *payload, int payload_len,
				  uint8_t *pkt_buf)
{
	pkt_buf[0] = src_id;
	sys_put_le16(payload_len, pkt_buf + 1);
	memcpy(pkt_buf + 3, payload, payload_len);

	return (sizeof(cs_router_data_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Load a UART packet from a buffer.
 *
 * @param pkt Pointer to instance of @ref cs_router_uart_packet, which should be loaded with data
 * @param buffer Buffer with data that should be created into an UART packet
 */
void PacketHandler::loadUartPacket(cs_router_uart_packet *uart_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	uart_pkt->start_token = buffer[pkt_ctr++];
	uart_pkt->length = sys_get_le16(buffer + pkt_ctr);
	pkt_ctr += 2;
	uart_pkt->protocol_version = buffer[pkt_ctr++];
	uart_pkt->type = buffer[pkt_ctr++];

	int payload_len = uart_pkt->length - 4; // minus protocol ver, type and CRC
	uart_pkt->payload = &buffer[pkt_ctr];
	pkt_ctr += payload_len;

	// check CRC CCITT over everything after length, not including CRC (uint16) itself
	uint16_t check_crc = crc16_ccitt(CS_PACKET_UART_CRC_SEED, buffer + 3, uart_pkt->length - 2);
	uint16_t received_crc = sys_get_le16(buffer + pkt_ctr);
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
void PacketHandler::loadGenericPacket(cs_router_generic_packet *generic_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	generic_pkt->protocol_version = buffer[pkt_ctr++];
	generic_pkt->type = buffer[pkt_ctr++];
	generic_pkt->length = sys_get_le16(buffer + pkt_ctr);
	pkt_ctr += 2;
	generic_pkt->payload = &buffer[pkt_ctr];

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
void PacketHandler::loadControlPacket(cs_router_control_packet *ctrl_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	ctrl_pkt->command_type = buffer[pkt_ctr++];
	ctrl_pkt->src_id = buffer[pkt_ctr++];
	ctrl_pkt->dest_id = buffer[pkt_ctr++];
	ctrl_pkt->length = sys_get_le16(buffer + pkt_ctr);
	pkt_ctr += 2;
	ctrl_pkt->payload = &buffer[pkt_ctr];
}

/**
 * @brief Load a switch command packet from a buffer.
 *
 * @param switch_pkt Pointer to instance of @ref cs_router_switch_command_packet, which should be
 * loaded with data
 * @param buffer Buffer with data that should be create into a switch command packet
 */
void PacketHandler::loadSwitchCommandPacket(cs_router_switch_command_packet *switch_pkt,
					    uint8_t *buffer)
{
	switch_pkt->switch_value = buffer[0];
}