/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"

#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>

#include <stdbool.h>
#include <stdint.h>

#define CS_HTTP_CLIENT_RECV_BUF_SIZE 1024
#define CS_HTTP_CLIENT_RECV_TIMEOUT  3000

#if IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define HTTP_PORT 443
#else
#define HTTP_PORT 80
#endif
#define HTTP_PROTOCOL_VER "HTTP/1.1"

class HttpClient : public Socket
{
public:
	HttpClient() = default;
	
	void init(struct cs_socket_opts *opts);
	cs_err_t sendHttpRequest(enum http_method method, const char *endpoint,
				 const char *payload);

private:
	/** Initialized flag */
	bool _initialized = false;

	/** Structure with socket options used to create a socket */
	struct cs_socket_opts *_opts = NULL;

	/** Buffer to store HTTP responses */
	uint8_t _http_recv_buf[CS_HTTP_CLIENT_RECV_BUF_SIZE];

	/** Mutex structure to lock access to the socket & response buffer */
	struct k_mutex _mtx;
};