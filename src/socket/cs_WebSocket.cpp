/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include "socket/cs_WebSocket.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_WebSocket, LOG_LEVEL_INF);

#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>

K_THREAD_STACK_DEFINE(ws_tid_stack_area, CS_WEBSOCKET_THREAD_STACK_SIZE);

/**
 * @brief Handle a websocket connection.
 */
static int handleWebsocketConnect(int ws_sock, http_request *req, void *user_data)
{
	WebSocket *ws_inst = static_cast<WebSocket *>(user_data);

	k_event_post(&ws_inst->_ws_evts, CS_WEBSOCKET_CONNECTED_EVENT);

	LOG_INF("Websocket %d connected", ws_sock);

	return 0;
}

/**
 * @brief Handle receiving messages on the websocket.
 * Runs in a dedicated thread.
 *
 * @param inst Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
static void handleMessageReceived(void *inst, void *unused1, void *unused2)
{
	WebSocket *ws_inst = static_cast<WebSocket *>(inst);

	uint32_t message_type;
	uint64_t remaining_bytes = ULLONG_MAX;

	// wait for connection to websocket before trying to receive messages from
	// peripherals
	k_event_wait(&ws_inst->_ws_evts, CS_WEBSOCKET_CONNECTED_EVENT, false, K_FOREVER);

	while (1) {
		int ret, read_pos = 0, total_read = 0;
		// receive data if available, don't block until it is
		while (remaining_bytes > 0) {
			ret = websocket_recv_msg(ws_inst->_websock_id,
						 (ws_inst->_ws_recv_buf + read_pos),
						 (sizeof(ws_inst->_ws_recv_buf) - read_pos),
						 &message_type, &remaining_bytes, 0);
			// there is still data available, try receiving the rest
			// or: there is no data available, wait for 50ms and reschedule
			if (ret < 0) {
				if (ret == -EAGAIN) {
					k_msleep(CS_WEBSOCKET_RECV_RETRY_TIMOUT);
					continue;
				}
				LOG_DBG("Websocket connection closed while waiting (%d/%d)", ret,
					errno);
				break;
			}
			read_pos += ret;
			total_read += ret;
		}

		LOG_DBG("Received %d bytes", total_read);

		// this struct is copied into the work handler
		cs_packet_data ws_data;
		memset(&ws_data, 0, sizeof(ws_data));
		ws_data.msg.buf_len = total_read;
		ws_data.type = CS_DATA_INCOMING;
		ws_data.src_id = ws_inst->_src_id;
		memcpy(&ws_data.msg.buf, ws_inst->_ws_recv_buf, total_read);

		if (ws_inst->_pkt_handler != NULL) {
			// dispatch the work item
			ws_inst->_pkt_handler->handlePacket(&ws_data);
		} else {
			LOG_WRN("%s", "Failed to handle websocket packet");
		}

		remaining_bytes = ULLONG_MAX;
	}
}

/**
 * @brief Open a websocket connection.
 *
 * @param url URL of the websocket, excluding the domain name or peer address and forward slash.
 * Max 30 characters, the rest is dropped. NULL if not required.
 *
 * @return CS_OK if connection is successful.
 */
cs_ret_code_t WebSocket::connect(const char *url)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	k_event_init(&_ws_evts);

	if (zsock_connect(_sock_id, &_addr, _addr_len) < 0) {
		LOG_ERR("Failed to connect to socket host with errno: %d", -errno);
		return CS_ERR_SOCKET_CONNECT_FAILED;
	}

	websocket_request ws_req;
	memset(&ws_req, 0, sizeof(ws_req));

	char url_buf[CS_WEBSOCKET_URL_MAX_LEN];
	char url_prefix[] = "/";
	strcpy(url_buf, url_prefix);
	// create url from forward slash + url if url provided
	if (url != NULL) {
		strncat(url_buf, url, (sizeof(url_buf) - sizeof(url_prefix)));
	}

	ws_req.host = _host;
	ws_req.url = url_buf;
	ws_req.cb = handleWebsocketConnect;
	ws_req.tmp_buf = _ws_recv_tmp_buf;
	ws_req.tmp_buf_len = sizeof(_ws_recv_tmp_buf);

	LOG_INF("Attempting connection to %s", _host);

	// perform http handshake for websocket connection, and open connection
	_websock_id = websocket_connect(_sock_id, &ws_req, SYS_FOREVER_MS, this);
	if (_websock_id < 0) {
		LOG_ERR("Failed to connect to websocket on %s%s", _host, url_buf);
		close();
		return CS_ERR_SOCKET_WEBSOCKET_CONNECT_FAILED;
	}

	// handle message receiving in a thread
	k_tid_t ws_recv_thread =
		k_thread_create(&_ws_tid, ws_tid_stack_area,
				K_THREAD_STACK_SIZEOF(ws_tid_stack_area), handleMessageReceived,
				this, NULL, NULL, CS_WEBSOCKET_THREAD_PRIORITY, 0, K_NO_WAIT);

	return CS_OK;
}

/**
 * @brief Send message over websocket. Callback function for PacketHandler.
 *
 * @param inst Pointer to WebSocket class instance.
 * @param message Pointer to buffer with the message.
 * @param len Length of the message.
 */
void WebSocket::sendMessage(k_work *work)
{
	cs_packet_handler *hdlr = CONTAINER_OF(work, cs_packet_handler, work_item);
	WebSocket *ws_inst = static_cast<WebSocket *>(hdlr->target_inst);
	k_spinlock_key_t key;
	int ret = 0;

	if (!ws_inst->_initialized) {
		LOG_ERR("%s", "Not initialized");
		return;
	}

	// wait for connection to websocket before trying to receive messages from peripherals
	k_event_wait(&ws_inst->_ws_evts, CS_WEBSOCKET_CONNECTED_EVENT, false, K_FOREVER);

	key = k_spin_lock(&hdlr->work_lock);
	// a message is available, make sure it is sent over the websocket
	ret = websocket_send_msg(ws_inst->_websock_id, hdlr->msg.buf, hdlr->msg.buf_len,
				 WEBSOCKET_OPCODE_DATA_TEXT, true, true, SYS_FOREVER_MS);
	k_spin_unlock(&hdlr->work_lock, key);
	if (ret < 0) {
		LOG_ERR("Could not send message over websocket (err %d)", ret);
	}

	LOG_DBG("Sent %d bytes", ret);
}

/**
 * @brief Close websocket & BSD socket.
 */
cs_ret_code_t WebSocket::close()
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_websock_id >= 0) {
		websocket_disconnect(_websock_id);
	}
	Socket::close();

	return CS_OK;
}