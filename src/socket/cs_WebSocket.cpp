/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include "socket/cs_WebSocket.h"
#include "cs_Router.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_WebSocket, LOG_LEVEL_DBG);

#include <zephyr/net/socket.h>
#include <zephyr/net/websocket.h>

#include <string.h>
#include <errno.h>
#include <limits.h>

K_THREAD_STACK_DEFINE(ws_tid_stack_area, CS_WEBSOCKET_THREAD_STACK_SIZE);

/**
 * @brief Handle a websocket connection.
 */
static int handleWebsocketConnect(int ws_sock, struct http_request *req, void *user_data)
{
	LOG_INF("Websocket %d for %s connected.", ws_sock, (char *)user_data);

	return 0;
}

/**
 * @brief Handle transport on the websocket.
 * Runs in a dedicated thread.
 *
 * @param cls Pointer to the class instance.
 * @param unused1 Unused parameter, is NULL.
 * @param unused2 Unused parameter, is NULL.
 */
static void handleTransport(void *cls, void *unused1, void *unused2)
{
	WebSocket *ws_obj = static_cast<WebSocket *>(cls);

	struct k_mbox_msg driver_msg;
	driver_msg.info = CS_WEBSOCKET_MBOX_BUF_SIZE;
	driver_msg.size = CS_WEBSOCKET_MBOX_BUF_SIZE;
	driver_msg.rx_source_thread = K_ANY;

	uint8_t driver_msg_buf[CS_WEBSOCKET_MBOX_BUF_SIZE];

	while (1) {
		if (ws_obj->_sock_id < 0 || ws_obj->_websock_id < 0) {
			break;
		}

		// get message from mailbox if available
		// don't block, as receiving needs to be handled as well
		if (k_mbox_get(&ws_obj->_ws_mbox, &driver_msg, driver_msg_buf, K_NO_WAIT) == 0) {
			// size is updated after message is retrieved
			if (driver_msg.info != driver_msg.size) {
				LOG_WRN("Data dropped when retrieving data from mailbox, %d bytes "
					"were sent, max is %d",
					driver_msg.info, CS_WEBSOCKET_MBOX_BUF_SIZE);
			}

			// a message is available, make sure it is sent over the websocket
			int send_ret = websocket_send_msg(
				ws_obj->_websock_id, driver_msg_buf, driver_msg.size,
				WEBSOCKET_OPCODE_DATA_TEXT, true, true, SYS_FOREVER_MS);
			if (send_ret < 0) {
				LOG_ERR("Error: %s occured while trying to send message over "
					"websocket",
					strerror(send_ret));
			}

			LOG_DBG("Sent %d bytes", send_ret);
		}

		int recv_ret, read_pos = 0, total_read = 0;
		uint32_t message_type;
		uint64_t remaining = ULLONG_MAX;
		// receive data if available, don't block until it is
		while (remaining > 0) {
			recv_ret = websocket_recv_msg(
				ws_obj->_websock_id, (ws_obj->_websocket_recv_buf + read_pos),
				(sizeof(ws_obj->_websocket_recv_buf) - read_pos), &message_type,
				&remaining, 0);
			if (recv_ret < 0) {
				if (recv_ret == -EAGAIN) {
					k_sleep(K_MSEC(CS_WEBSOCKET_RECV_RETRY_TIMOUT));
					continue;
				}
				LOG_DBG("Websocket connection closed while waiting (%d/%d)",
					recv_ret, errno);
				break;
			}
			read_pos += recv_ret;
			total_read += recv_ret;
		}

		LOG_DBG("Received %d bytes", total_read);

		// sleep for 100 ms to yield control to other threads
		k_sleep(K_MSEC(CS_WEBSOCKET_THREAD_SLEEP));
	}
}

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

	k_mbox_init(&_ws_mbox);

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Open a websocket connection.
 *
 * @param url URL of the websocket, excluding the domain name or peer address and forward slash.
 * NULL if not required.
 *
 * @return CS_OK if connection is successful.
 */
cs_err_t WebSocket::connect(const char *url)
{
	if (!_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (zsock_connect(_sock_id, _addr, _addr_len) < 0) {
		LOG_ERR("Failed to connect to socket host: %d", -errno);
		return CS_ERR_SOCKET_CONNECT_FAILED;
	}

	struct websocket_request ws_req;
	memset(&ws_req, 0, sizeof(ws_req));

	char url_prefix[] = "/";

	ws_req.host = _host_name;
	ws_req.url = url == NULL ? url_prefix : strcat(url_prefix, url);
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
	_ws_thread = k_thread_create(&_ws_tid, ws_tid_stack_area,
				     K_THREAD_STACK_SIZEOF(ws_tid_stack_area), handleTransport,
				     this, NULL, NULL, CS_WEBSOCKET_THREAD_PRIORITY, 0, K_NO_WAIT);

	LOG_INF("Opened websocket connection on %s/%s", _host_name, url);

	return CS_OK;
}

/**
 * @brief Send message over websocket asynchronously by putting it in a mailbox.
 *
 * @param message Pointer to buffer with the message.
 * @param len Length of the message.
 * @param sem Optional pointer to a semaphore that will be given when the message has been received.
 * NULL if not needed.
 *
 * @return CS_OK if the message was succesfully put into the mailbox.
 */
cs_err_t WebSocket::sendMessage(uint8_t *message, size_t len, struct k_sem *sem)
{
	if (!_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	struct k_mbox_msg send_msg;

	send_msg.info = len;
	send_msg.size = len;
	send_msg.tx_data = message;
	send_msg.tx_block.data = NULL;
	send_msg.tx_target_thread = _ws_thread;

	k_mbox_async_put(&_ws_mbox, &send_msg, sem);

	return CS_OK;
}

/**
 * @brief Close websocket & BSD socket.
 */
cs_err_t WebSocket::close()
{
	if (!_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_websock_id >= 0) {
		websocket_disconnect(_websock_id);
	}
	closeSocket();

	return CS_OK;
}