/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"

#define CS_HTTP_CLIENT_RECV_BUF_SIZE 1024
#define CS_HTTP_CLIENT_RECV_TIMEOUT  3000

#if IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define HTTP_PORT 443
#else
#define HTTP_PORT 80
#endif
#define HTTP_PROTOCOL_VER "HTTP/1.1"

class HttpClient : Socket
{
public:
	void init(struct cs_socket_opts *opts);
	cs_err_t sendHttpRequest(enum http_method method, const char *endpoint,
				 const char *payload);

private:
	static void handleHttpResponse(struct http_response *rsp, enum http_final_call final_data,
				       void *user_data);

	bool _is_initialized = false;
	
	struct cs_socket_opts *_opts = NULL;
	uint8_t _http_recv_buf[CS_HTTP_CLIENT_RECV_BUF_SIZE];
};