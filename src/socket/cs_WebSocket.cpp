/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include <string.h>
#include <climits>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_WebSocket, LOG_LEVEL_INF);

#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include "cs_Router.h"
#include "socket/cs_WebSocket.h"

K_THREAD_STACK_DEFINE(ws_tid_stack_area, CS_WEBSOCKET_THREAD_STACK_SIZE);

/**
 * @brief Initialize websocket.
 *
 * @param opts Instance of @ref cs_socket_opts.
 *
 * @return CS_OK if the initialization was succesful.
 */
cs_err_t WebSocket::init(struct cs_socket_opts *opts)
{
	int ret = initSocket(opts);
	if (ret != CS_OK) {
		return ret;
	}

	_is_initialized = true;

	return CS_OK;
}

/**
 * @brief Open a websocket connection.
 *
 * @return CS_OK if connection is successful.
 */
cs_err_t WebSocket::connect(const char *url)
{
	if (!_is_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (zsock_connect(_sock_id, _addr, _addr_len) < 0) {
		LOG_ERR("Failed to connect to socket host %d", -errno);
		return CS_ERR_SOCKET_CONNECT_FAILED;
	}

	struct websocket_request ws_req;
	memset(&ws_req, 0, sizeof(ws_req));

	char url_prefix[] = "/";

	ws_req.host = _host_name;
	ws_req.url = strcat(url_prefix, url);
	ws_req.cb = handleWebsocketConnect;
	ws_req.tmp_buf = _websocket_recv_temp_buf;
	ws_req.tmp_buf_len = sizeof(_websocket_recv_temp_buf);

	// perform http handshake for websocket connection
	_websock_id = websocket_connect(_sock_id, &ws_req, CS_WEBSOCKET_RECV_TIMEOUT, &_host_name);
	if (_websock_id < 0) {
		LOG_ERR("Failed to connect to websocket on %s", _host_name);
		zsock_close(_sock_id);
		return CS_ERR_SOCKET_WEBSOCKET_CONNECT_FAILED;
	}

	// handle sending and receiving in a thread
	struct k_thread ws_tid;
	k_tid_t ws_thread = k_thread_create(
		&ws_tid, ws_tid_stack_area, K_THREAD_STACK_SIZEOF(ws_tid_stack_area),
		handleTransport, this, NULL, NULL, CS_WEBSOCKET_THREAD_PRIORITY, 0, K_NO_WAIT);

	LOG_INF("Opened websocket connection on %s/%s", _host_name, url);

	return CS_OK;
}

/**
 * @brief Handle a websocket connection.
 */
cs_err_t WebSocket::handleWebsocketConnect(int sock, struct http_request *req, void *user_data)
{
	LOG_INF("Websocket %d for %s connected.", sock, (char *)user_data);

	return CS_OK;
}

/**
 * @brief Handle transport on the websocket.
 * Runs in a dedicated thread.
 * 
 * @param cls Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
void WebSocket::handleTransport(void *cls, void *unused1, void *unused2)
{
	WebSocket *ws_obj = (WebSocket *)cls;

	while (1) {
		if (ws_obj->_sock_id < 0 || ws_obj->_websock_id < 0) {
			break;
		}

		// check events for UART RS / BLE data received
		// TODO send packets

		int ret, read_pos = 0, total_read = 0;
		uint32_t message_type;
		uint64_t remaining = ULLONG_MAX;
		// receive data if available, don't block until it is
		while (remaining > 0) {
			ret = websocket_recv_msg(ws_obj->_websock_id,
						 (ws_obj->_websocket_recv_buf + read_pos),
						 (sizeof(ws_obj->_websocket_recv_buf) - read_pos),
						 &message_type, &remaining, 0);
			if (ret <= 0) {
				if (ret == -EAGAIN) {
					k_sleep(K_MSEC(CS_WEBSOCKET_RECV_RETRY_TIMOUT));
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

		// sleep and yield cpu to other threads
		k_sleep(K_MSEC(CS_WEBSOCKET_THREAD_SLEEP));
	}
}

/**
 * @brief Close websocket & BSD socket.
 */
cs_err_t WebSocket::close()
{
	if (!_is_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_websock_id >= 0) {
		websocket_disconnect(_websock_id);
	}
	closeSocket();

	return CS_OK;
}