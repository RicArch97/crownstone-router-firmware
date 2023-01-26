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
static void handleIncomingPacket(k_work *work)
{
	cs_packet_handler_input *in_handler =
		CONTAINER_OF(work, cs_packet_handler_input, work_item);

	PacketHandler *ph_inst = static_cast<PacketHandler *>(in_handler->ph_inst);

	cs_router_generic_packet generic_pkt;

	// for safety: the system queue is generally threadsafe,
	// but if we somehow can't get access: requeue the work
	if (k_sem_take(&in_handler->work_sem, K_NO_WAIT) != 0) {
		k_work_submit(work);
		return;
	}

	if (in_handler->source_id == CS_INSTANCE_ID_UART_CM4) {
		cs_router_uart_packet uart_pkt;
		loadUartPacket(&uart_pkt, in_handler->buffer);
		loadGenericPacket(&generic_pkt, uart_pkt.payload);
	} else {
		loadGenericPacket(&generic_pkt, in_handler->buffer);
	}

	if (generic_pkt.type == CS_PACKET_TYPE_CONTROL) {
		cs_router_control_packet ctrl_pkt;
		loadControlPacket(&ctrl_pkt, generic_pkt.payload);

		cs_packet_data out;
		cs_packet_handler_output *outh;

		switch (ctrl_pkt.command_type) {
		case CS_COMMAND_TYPE_REQUEST:
			// request is given, so we must respond with a result
			outh = ph_inst->getOutputHandler((cs_router_instance_id)ctrl_pkt.src_id);
			// we need to write to (possibly shared) memory
			if (k_sem_take(&outh->work_sem, K_NO_WAIT) != 0) {
				LOG_ERR("%s", "Could not take semaphore for result output handler");
				return;
			}
			// set the result id and change the target id for our destination device
			outh->result_id = ctrl_pkt.request_id;
			outh->target_id = (cs_router_instance_id)ctrl_pkt.src_id;
			k_sem_give(&outh->work_sem);
			__fallthrough;
		case CS_COMMAND_TYPE_SWITCH:
			// transport data to peripheral
			out.type = CS_DATA_OUTGOING;
			out.buffer = ctrl_pkt.payload;
			out.buffer_len = ctrl_pkt.length;
			out.dest_id = (cs_router_instance_id)ctrl_pkt.dest_id;
			out.src_id = (cs_router_instance_id)ctrl_pkt.src_id;

			// dispatch packet (this copies the data first)
			ph_inst->handlePacket(&out);
			break;
		}
	}
	k_sem_give(&in_handler->work_sem);
}

/**
 * @brief Handler for data coming from peripherals.
 */
static void handleOutgoingPacket(k_work *work)
{
	cs_packet_handler_output *out_handler =
		CONTAINER_OF(work, cs_packet_handler_output, work_item);

	// for safety: the system queue is generally threadsafe,
	// but if we somehow can't get access: requeue the work
	if (k_sem_take(&out_handler->work_sem, K_NO_WAIT) != 0) {
		k_work_submit(work);
		return;
	}

	uint8_t pkt_buf[CS_PACKET_BUF_SIZE];
	int pkt_len;
	cs_router_generic_packet_type pkt_type;

	// if a result id was set by the incoming packet handler,
	// create a result packet for request
	if (out_handler->result_id > 0) {
		pkt_len = wrapResultPacket(CS_COMMAND_TYPE_REQUEST, out_handler->result_code,
					   out_handler->result_id, out_handler->buffer,
					   out_handler->buffer_len, pkt_buf);
		pkt_type = CS_PACKET_TYPE_RESULT;
	} else {
		// all other data is wrapped as "data", the contents are unknown
		pkt_len = wrapDataPacket(out_handler->source_id, out_handler->buffer,
					 out_handler->buffer_len, pkt_buf);
		pkt_type = CS_PACKET_TYPE_DATA;
	}

	k_sem_give(&out_handler->work_sem);

	uint8_t tmp_buf[pkt_len];
	memcpy(tmp_buf, pkt_buf, pkt_len);

	int generic_pkt_len = wrapGenericPacket(pkt_type, tmp_buf, pkt_len, pkt_buf);
	pkt_len = generic_pkt_len;

	// when packet should be routed to CM4
	if (out_handler->target_id == CS_INSTANCE_ID_UART_CM4) {
		uint8_t generic_pkt_tmp_buf[generic_pkt_len];
		memcpy(generic_pkt_tmp_buf, pkt_buf, generic_pkt_len);

		pkt_len = wrapUartPacket(CS_PACKET_TYPE_GENERIC, generic_pkt_tmp_buf,
					 generic_pkt_len, pkt_buf);
	}

	// send the packet to the target
	out_handler->cb(out_handler->target_inst, pkt_buf, pkt_len);
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

	k_mutex_init(&_out_pkth_mtx);
	k_mutex_init(&_in_pkth_mtx);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Register incoming handler used for incoming data.
 *
 * @param inst_id Instance ID of the instance that retrieved data.
 */
cs_ret_code_t PacketHandler::registerInputHandler(cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_mutex_lock(&_in_pkth_mtx, K_FOREVER);

	for (int i = 0; i < _in_handler_ctr; i++) {
		if (_in_handlers[i].source_id == inst_id) {
			LOG_ERR("Handler with ID %d already registered", inst_id);
			return CS_ERR_PACKET_HANDLER_ALREADY_REGISTERED;
		}
	}
	// create a new handler
	cs_packet_handler_input in_handler;
	memset(&in_handler, 0, sizeof(in_handler));
	in_handler.source_id = inst_id;
	in_handler.ph_inst = this;

	k_work_init(&in_handler.work_item, handleIncomingPacket);
	k_sem_init(&in_handler.work_sem, 1, 1);

	_in_handlers[_in_handler_ctr++] = in_handler;

	k_mutex_unlock(&_in_pkth_mtx);

	return CS_OK;
}

/**
 * @brief Register a handler that can be used for outgoing data.
 *
 * @param inst_id Instance ID of the destination instance that should handle the data.
 * @param inst Pointer to class instance according to destination ID.
 * @param cb Function pointer to function that should be called to transport the data.
 */
cs_ret_code_t PacketHandler::registerOutputHandler(cs_router_instance_id inst_id, void *inst,
						   cs_packet_transport_cb_t cb)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_mutex_lock(&_out_pkth_mtx, K_FOREVER);

	for (int i = 0; i < _out_handler_ctr; i++) {
		if (_out_handlers[i].target_id == inst_id) {
			LOG_ERR("Handler with ID %d already registered", inst_id);
			return CS_ERR_PACKET_HANDLER_ALREADY_REGISTERED;
		}
	}
	// create a new handler
	cs_packet_handler_output out_handler;
	memset(&out_handler, 0, sizeof(out_handler));
	out_handler.target_id = inst_id;
	out_handler.target_inst = inst;
	out_handler.cb = cb;

	k_work_init(&out_handler.work_item, handleOutgoingPacket);
	k_sem_init(&out_handler.work_sem, 1, 1);

	// store handler
	_out_handlers[_out_handler_ctr++] = out_handler;

	k_mutex_unlock(&_out_pkth_mtx);

	return CS_OK;
}

/**
 * @brief Unregister a registered data handler.
 *
 * @param inst_id Instance ID that was registered for handling data.
 */
cs_ret_code_t PacketHandler::unregisterHandler(cs_packet_transport_type type,
					       cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	int i, j;

	switch (type) {
	case CS_DATA_INCOMING:
		k_mutex_lock(&_in_pkth_mtx, K_FOREVER);

		for (i = 0; i < _in_handler_ctr; i++) {
			if (_in_handlers[i].source_id == inst_id) {
				// clean from register by shifting next data to removed slot
				for (j = (i - 1); i < (_in_handler_ctr - 1); i++) {
					_in_handlers[j] = _in_handlers[j + 1];
				}
				_in_handler_ctr--;
				return CS_OK;
			}
		}
		k_mutex_unlock(&_in_pkth_mtx);

		LOG_ERR("Could not find incoming data handler for ID %d", inst_id);
		break;
	case CS_DATA_OUTGOING:
		k_mutex_lock(&_out_pkth_mtx, K_FOREVER);

		for (i = 0; i < _out_handler_ctr; i++) {
			if (_out_handlers[i].target_id == inst_id) {
				// clean from register by shifting next data to removed slot
				for (j = (i - 1); i < (_out_handler_ctr - 1); i++) {
					_out_handlers[j] = _out_handlers[j + 1];
				}
				_out_handler_ctr--;
				return CS_OK;
			}
		}
		k_mutex_unlock(&_out_pkth_mtx);

		LOG_ERR("Could not find outgoing data handler for ID %d", inst_id);
		break;
	}
	return CS_ERR_PACKET_HANDLER_NOT_FOUND;
}

/**
 * @brief Get an output handler by it's target ID. Used by the input handler
 * to add result data.
 *
 * @param inst_id Instance ID of the destination instance that should handle the data.
 */
cs_packet_handler_output *PacketHandler::getOutputHandler(cs_router_instance_id inst_id)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return NULL;
	}

	for (int i = 0; i < _out_handler_ctr; i++) {
		if (_out_handlers[i].target_id == inst_id) {
			return &_out_handlers[i];
		}
	}
	LOG_ERR("Could not find outgoing handler for ID %d", inst_id);
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

	int i;

	switch (data->type) {
	case CS_DATA_INCOMING:
		for (i = 0; i < _in_handler_ctr; i++) {
			if (_in_handlers[i].source_id == data->src_id) {
				int len = MIN(data->buffer_len, sizeof(_in_handlers[i].buffer));
				memcpy(_in_handlers[i].buffer, data->buffer, len);
				_in_handlers[i].source_id = data->src_id;

				k_work_submit(&_in_handlers[i].work_item);
				return CS_OK;
			}
		}
		LOG_ERR("Could not find incoming data handler for ID %d", data->src_id);
		break;
	case CS_DATA_OUTGOING:
		for (i = 0; i < _out_handler_ctr; i++) {
			if (_out_handlers[i].target_id == data->dest_id) {
				int len = MIN(data->buffer_len, sizeof(_out_handlers[i].buffer));
				memcpy(_out_handlers[i].buffer, data->buffer, len);
				_out_handlers[i].buffer_len = len;
				_out_handlers[i].source_id = data->src_id;

				k_work_submit(&_out_handlers[i].work_item);
				return CS_OK;
			}
		}
		LOG_ERR("Could not find outgoing handler for ID %d", data->dest_id);
		break;
	}
	return CS_ERR_PACKET_HANDLER_NOT_FOUND;
}