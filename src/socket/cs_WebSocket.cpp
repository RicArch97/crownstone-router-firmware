/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Socket, LOG_LEVEL_INF);

#include "cs_ReturnTypes.h"
#include "cs_Router.h"
#include "socket/cs_WebSocket.h"

#include <zephyr/kernel.h>
#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include <zephyr/net/tls_credentials.h>
#include "socket/cs_CaCertificate.h"

K_THREAD_STACK_DEFINE(ws_tid_stack_area, THREAD_STACK_SIZE);

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
	_websock_id = websocket_connect(_sock_id, &ws_req, CS_SOCKET_RECV_TIMEOUT, &_host_name);
	if (_websock_id < 0) {
		LOG_ERR("Failed to connect to websocket on %s", _host_name);
		zsock_close(_sock_id);
		return CS_ERR_SOCKET_WEBSOCKET_CONNECT_FAILED;
	}

	// handle sending and receiving in a thread
	struct k_thread ws_tid;
	k_tid_t ws_thread = k_thread_create(
		&ws_tid, ws_tid_stack_area, K_THREAD_STACK_SIZEOF(ws_tid_stack_area),
		handleTransport, this, NULL, NULL, THREAD_PRIORITY, 0, K_NO_WAIT);

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
					k_sleep(K_MSEC(CS_SOCKET_RECV_RETRY_TIMOUT));
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
		k_sleep(K_MSEC(THREAD_SLEEP));
	}
}

/**
 * @brief Close websocket & BSD socket.
 */
cs_err_t WebSocket::disconnect()
{
	if (!_is_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_websock_id >= 0) {
		websocket_disconnect(_websock_id);
	}
	if (_sock_id >= 0) {
		zsock_close(_sock_id);
	}

	return CS_OK;
}

/**
 * @brief Send a HTTP request over a socket.
 *
 * @param method One of @ref http_method, e.g. HTTP_GET, HTTP_POST
 * @param endpoint Endpoint / url of the request
 * @param payload Payload that should be send to the server, only for POST, otherwise NULL
 */
cs_err_t WebSocket::sendHttpRequest(enum http_method method, const char *endpoint,
				    const char *payload)
{
	struct http_request http_req;
	memset(&http_req, 0, sizeof(http_req));

	char url_prefix[] = "/";

	http_req.method = method;
	http_req.url = strcat("/", endpoint);
	http_req.host = _host_name;
	http_req.protocol = HTTP_PROTOCOL_VER;
	http_req.recv_buf = _socket_recv_buf;
	http_req.recv_buf_len = CS_SOCKET_RECV_BUF_SIZE;

	if (method == HTTP_POST && payload != NULL) {
		http_req.payload = payload;
		http_req.payload_len = strlen(payload);
	}

	if (http_client_req(_sock_id, &http_req, CS_SOCKET_RECV_TIMEOUT, this) < 0) {
		LOG_ERR("Failed to send http POST request");
		return CS_ERR_SOCKET_HTTP_GET_FAILED;
	}

	return CS_OK;
}