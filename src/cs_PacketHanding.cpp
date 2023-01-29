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

K_THREAD_STACK_DEFINE(packet_tid_stack_area, CS_PACKET_THREAD_STACK_SIZE);

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
static int wrapUartPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
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
static int wrapGenericPacket(uint8_t type, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
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
static int wrapDataPacket(uint8_t src_id, uint8_t *payload, int payload_len, uint8_t *pkt_buf)
{
	pkt_buf[0] = src_id;
	sys_put_le16(payload_len, pkt_buf + 1);
	memcpy(pkt_buf + 3, payload, payload_len);

	return (sizeof(cs_router_data_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Wrap a payload into a result packet.
 */
static int wrapResultPacket(uint8_t command_type, uint8_t result_code, uint16_t request_id,
			    uint8_t *payload, uint16_t payload_len, uint8_t *pkt_buf)
{
	int result_pkt_ctr = 0;

	pkt_buf[result_pkt_ctr++] = command_type;
	pkt_buf[result_pkt_ctr++] = result_code;
	sys_put_le16(request_id, pkt_buf + result_pkt_ctr);
	result_pkt_ctr += 2;
	sys_put_le16(payload_len, pkt_buf + result_pkt_ctr);
	result_pkt_ctr += 2;
	memcpy(pkt_buf + result_pkt_ctr, payload, payload_len);

	return (sizeof(cs_router_result_packet) - sizeof(uint8_t *)) + payload_len;
}

/**
 * @brief Load a UART packet from a buffer.
 *
 * @param pkt Pointer to instance of @ref cs_router_uart_packet, which should be loaded with data
 * @param buffer Buffer with data that should be created into an UART packet
 */
static void loadUartPacket(cs_router_uart_packet *uart_pkt, uint8_t *buffer)
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
static void loadGenericPacket(cs_router_generic_packet *generic_pkt, uint8_t *buffer)
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
static void loadControlPacket(cs_router_control_packet *ctrl_pkt, uint8_t *buffer)
{
	int pkt_ctr = 0;

	ctrl_pkt->command_type = buffer[pkt_ctr++];
	ctrl_pkt->src_id = buffer[pkt_ctr++];
	ctrl_pkt->dest_id = buffer[pkt_ctr++];
	ctrl_pkt->request_id = sys_get_le16(buffer + pkt_ctr);
	pkt_ctr += 2;
	ctrl_pkt->length = sys_get_le16(buffer + pkt_ctr);
	pkt_ctr += 2;
	ctrl_pkt->payload = &buffer[pkt_ctr];
}

/**
 * @brief Handler for an incoming packet, from either CM4 or cloud.
 */
static void handleIncomingPacket(cs_packet_data *data, void *pkth)
{
	PacketHandler *ph_inst = static_cast<PacketHandler *>(pkth);
	cs_router_generic_packet generic_pkt;

	if (data->src_id == CS_INSTANCE_ID_UART_CM4) {
		cs_router_uart_packet uart_pkt;
		loadUartPacket(&uart_pkt, data->msg);
		loadGenericPacket(&generic_pkt, uart_pkt.payload);
	} else {
		loadGenericPacket(&generic_pkt, data->msg);
	}

	if (generic_pkt.type == CS_PACKET_TYPE_CONTROL) {
		cs_router_control_packet ctrl_pkt;
		loadControlPacket(&ctrl_pkt, generic_pkt.payload);

		cs_packet_data out;
		memset(&out, 0, sizeof(out));
		cs_packet_handler *outh =
			ph_inst->getHandler((cs_router_instance_id)ctrl_pkt.dest_id);

		switch (ctrl_pkt.command_type) {
		case CS_COMMAND_TYPE_REQUEST:
			// got a request, need to rely with a result
			outh->result_id = ctrl_pkt.request_id;
			__fallthrough;
		case CS_COMMAND_TYPE_SWITCH:
			// dispatch data to peripheral
			outh->cb(outh->target_inst, ctrl_pkt.payload, ctrl_pkt.length);
			break;
		}
	}
}

/**
 * @brief Handler for data coming from peripherals.
 */
static void handleOutgoingPacket(cs_packet_data *data, void *pkth)
{
	PacketHandler *ph_inst = static_cast<PacketHandler *>(pkth);
	uint8_t pkt_buf[CS_PACKET_BUF_SIZE];
	int pkt_len;
	cs_router_generic_packet_type pkt_type;

	// if a result id was set by the incoming packet handler,
	// create a result packet for request
	uint16_t *result_id = ph_inst->getResultId(data->src_id);
	if (result_id != NULL && *result_id > 0) {
		pkt_len = wrapResultPacket(CS_COMMAND_TYPE_REQUEST, data->result_code, *result_id,
					   data->msg, data->msg_len, pkt_buf);
		pkt_type = CS_PACKET_TYPE_RESULT;
		// request handled, reset the result id
		*result_id = 0;
	} else {
		// all other data is wrapped as "data", the contents are unknown
		pkt_len = wrapDataPacket(data->src_id, data->msg, data->msg_len, pkt_buf);
		pkt_type = CS_PACKET_TYPE_DATA;
	}

	uint8_t tmp_buf[pkt_len];
	memcpy(tmp_buf, pkt_buf, pkt_len);

	int generic_pkt_len = wrapGenericPacket(pkt_type, tmp_buf, pkt_len, pkt_buf);
	pkt_len = generic_pkt_len;

	// when packet should be routed to CM4
	if (data->dest_id == CS_INSTANCE_ID_UART_CM4) {
		uint8_t generic_pkt_tmp_buf[generic_pkt_len];
		memcpy(generic_pkt_tmp_buf, pkt_buf, generic_pkt_len);

		pkt_len = wrapUartPacket(CS_PACKET_TYPE_GENERIC, generic_pkt_tmp_buf,
					 generic_pkt_len, pkt_buf);
	}

	cs_packet_handler *outh = ph_inst->getHandler(data->dest_id);
	// dispatch packet to the target
	outh->cb(outh->target_inst, pkt_buf, pkt_len);
}

/**
 * @brief Thread function that handles packet buffers from the message queue.
 *
 * @param inst Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
static void handlePacketBuffers(void *inst, void *unused1, void *unused2)
{
	PacketHandler *pkth_inst = static_cast<PacketHandler *>(inst);
	cs_packet_data pkt_data;

	while (1) {
		// wait till message is retrieved from message queue
		if (k_msgq_get(&pkth_inst->_pkth_msgq, &pkt_data, K_FOREVER) == 0) {
			switch (pkt_data.type) {
			case CS_DATA_INCOMING:
				handleIncomingPacket(&pkt_data, pkth_inst);
				break;
			case CS_DATA_OUTGOING:
				handleOutgoingPacket(&pkt_data, pkth_inst);
				break;
			}
		} else {
			LOG_WRN("%s", "Failed to retrieve message from message queue");
		}
	}
}

/**
 * @brief Initialize PacketHandler instance.
 */
cs_ret_code_t PacketHandler::init()
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	k_mutex_init(&_pkth_mtx);
	// initialize message queue, aligned to 4-byte boundary
	k_msgq_init(&_pkth_msgq, _msgq_buf, sizeof(cs_packet_data), CS_PACKET_BUF_SIZE);

	// create thread for handling uart messages
	k_tid_t uart_thread = k_thread_create(
		&_pkth_tid, packet_tid_stack_area, K_THREAD_STACK_SIZEOF(packet_tid_stack_area),
		handlePacketBuffers, this, NULL, NULL, CS_PACKET_THREAD_PRIORITY, 0, K_NO_WAIT);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Register a handler that can be used for outgoing data.
 *
 * @param inst_id Instance ID of the destination instance that should handle the data.
 * @param inst Pointer to class instance according to destination ID.
 * @param cb Function pointer to function that should be called to transport the data.
 */
cs_ret_code_t PacketHandler::registerHandler(cs_router_instance_id inst_id, void *inst,
					     cs_packet_transport_cb_t cb)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_mutex_lock(&_pkth_mtx, K_FOREVER);

	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			LOG_ERR("Handler with ID %d already registered", inst_id);
			return CS_ERR_PACKET_HANDLER_ALREADY_REGISTERED;
		}
	}
	// create a new handler
	cs_packet_handler handler;
	memset(&handler, 0, sizeof(handler));
	handler.id = inst_id;
	handler.cb = cb;
	handler.target_inst = inst;

	// store handler
	memcpy(&_handlers[_handler_ctr++], &handler, sizeof(handler));

	k_mutex_unlock(&_pkth_mtx);

	return CS_OK;
}

/**
 * @brief Unregister a registered data handler.
 *
 * @param inst_id Instance ID that was registered for handling data.
 */
cs_ret_code_t PacketHandler::unregisterHandler(cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_mutex_lock(&_pkth_mtx, K_FOREVER);

	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			// clean from register by shifting next data to removed slot
			for (int j = (i - 1); i < (_handler_ctr - 1); i++) {
				_handlers[j] = _handlers[j + 1];
			}
			_handler_ctr--;
			return CS_OK;
		}
	}
	k_mutex_unlock(&_pkth_mtx);

	LOG_ERR("Could not find input handler for ID %d", inst_id);

	return CS_ERR_PACKET_HANDLER_NOT_FOUND;
}

/**
 * @brief Get an output handler by it's target ID. Used by the input handler
 * to pass data to peripherals.
 *
 * @param inst_id Instance ID of the destination instance that should handle the data.
 */
cs_packet_handler *PacketHandler::getHandler(cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return NULL;
	}

	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			return &_handlers[i];
		}
	}
	LOG_ERR("Could not find handler for ID %d", inst_id);
	return NULL;
}

/**
 * @brief Get a result ID for a handler, to check if it was set.
 * In that case the receiver could reply with a result packet.
 *
 * @param inst_id Instance ID of the instance to check the result id of.
 *
 * @return Pointer to result ID for a handler, or NULL if not found.
 */
uint16_t *PacketHandler::getResultId(cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return NULL;
	}

	for (int i = 0; i < _handler_ctr; i++) {
		if (_handlers[i].id == inst_id) {
			return &_handlers[i].result_id;
		}
	}
	LOG_ERR("Result ID: Could not find output handler for ID %d", inst_id);
	return NULL;
}

/**
 * @brief Handle packet by calling the associated callback for an instance.
 *
 * @param data Structure with packet data. One of @ref cs_packet_data
 *
 * @return CS_OK if the packet was dispatched successfully.
 */
cs_ret_code_t PacketHandler::handlePacket(cs_packet_data *data)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (k_msgq_num_used_get(&_pkth_msgq) == CS_PACKET_QUEUE_SIZE) {
		LOG_WRN("%s", "Failed to submit message to packet handler queue, queue is full");
		return CS_ERR_PACKET_HANDLER_NOT_READY;
	};

	// dispatch the packet data to be handled later (async)
	if (k_msgq_put(&_pkth_msgq, data, K_FOREVER) != 0) {
		LOG_WRN("%s", "Failed to submit message to packet handler queue");
		return CS_ERR_PACKET_HANDLER_NOT_READY;
	};

	return CS_OK;
}