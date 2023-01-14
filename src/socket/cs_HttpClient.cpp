/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include "socket/cs_HttpClient.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_HttpClient, LOG_LEVEL_INF);

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <string.h>

/**
 * @brief Handle a HTTP response.
 */
static void handleHttpResponse(http_response *rsp, http_final_call final_data, void *user_data)
{
	if (final_data == HTTP_DATA_MORE) {
		LOG_DBG("Partial data received (%zd bytes)", rsp->data_len);
	} else if (final_data == HTTP_DATA_FINAL) {
		LOG_DBG("Received http data (%zd bytes)", rsp->data_len);
	}

	LOG_INF("Request to endpoint %s got response status %s", (char *)user_data,
		rsp->http_status);
}

/**
 * @brief Initialize websocket.
 *
 * @param opts Instance of @ref cs_socket_opts.
 */
void HttpClient::init(cs_socket_opts *opts)
{
	_opts = opts;

	k_mutex_init(&_mtx);

	_initialized = true;
}

/**
 * @brief Send a HTTP request over a socket.
 *
 * @param method One of @ref http_method, e.g. HTTP_GET, HTTP_POST
 * @param endpoint Endpoint / url of the request
 * @param payload Payload that should be send to the server, otherwise NULL
 */
cs_err_t HttpClient::sendHttpRequest(http_method method, const char *endpoint, const char *payload)
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	// make sure only 1 request has access to the socket
	k_mutex_lock(&_mtx, K_FOREVER);

	// create new socket for the request
	cs_err_t ret = Socket::init(_opts);
	if (ret != CS_OK) {
		return ret;
	}

	if (zsock_connect(_sock_id, _addr, _addr_len) < 0) {
		LOG_ERR("Failed to connect to socket host %d", -errno);
		return CS_ERR_SOCKET_CONNECT_FAILED;
	}

	http_request http_req;
	memset(&http_req, 0, sizeof(http_req));

	char url_prefix[] = "/";

	http_req.method = method;
	http_req.url = strcat(url_prefix, endpoint);
	http_req.host = _host_name;
	http_req.protocol = HTTP_PROTOCOL_VER;
	http_req.recv_buf = _http_recv_buf;
	http_req.recv_buf_len = CS_HTTP_CLIENT_RECV_BUF_SIZE;
	http_req.response = handleHttpResponse;

	if (payload != NULL) {
		http_req.payload = payload;
		http_req.payload_len = strlen(payload);
	}

	if (http_client_req(_sock_id, &http_req, CS_HTTP_CLIENT_RECV_TIMEOUT, (char *)endpoint) <
	    0) {
		LOG_ERR("%s", "Failed to send http request");
		return CS_ERR_SOCKET_HTTP_REQ_FAILED;
	}

	Socket::close();

	k_mutex_unlock(&_mtx);

	return CS_OK;
}