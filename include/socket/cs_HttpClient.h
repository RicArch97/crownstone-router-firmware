/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdbool.h>

#include <zephyr/kernel.h>
#include <zephyr/net/http/client.h>

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"

#define CS_HTTP_CLIENT_RECV_BUF_SIZE 512
#define CS_HTTP_CLIENT_RECV_TIMEOUT  3000

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define HTTP_PORT 443
#else
#define HTTP_PORT 80
#endif
#define HTTP_PROTOCOL_VER "HTTP/1.1"

class HttpClient : Socket
{
      public:
	cs_err_t sendHttpRequest(enum http_method method, const char *endpoint,
				 const char *payload);

      private:
	struct cs_socket_opts *_opts = NULL;

	uint8_t _http_recv_buf[CS_HTTP_CLIENT_RECV_BUF_SIZE];
};