/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdbool.h>

#include <zephyr/kernel.h>

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"

#define THREAD_PRIORITY	  K_PRIO_PREEMPT(8)
#define THREAD_STACK_SIZE 1024
#define THREAD_SLEEP	  250

#define CS_SOCKET_RECV_BUF_SIZE	    512
#define CS_SOCKET_HTTP_HEADER_SIZE  30
#define CS_SOCKET_RECV_TIMEOUT	    3000
#define CS_SOCKET_RECV_RETRY_TIMOUT 50

class WebSocket : Socket
{
      public:
	WebSocket();
	WebSocket(struct cs_socket_opts *opts)
	{
		init(opts);
	}

	cs_err_t connect(const char *url);
	cs_err_t disconnect();

      private:
	static cs_err_t handleWebsocketConnect(int sock, struct http_request *req, void *user_data);
	static void handleTransport(void *cls, void *unused1, void *unused2);

	struct cs_socket_opts *_opts = NULL;
	int _websock_id = -1;

	uint8_t _websocket_recv_buf[CS_SOCKET_RECV_BUF_SIZE];
	uint8_t _websocket_recv_temp_buf[CS_SOCKET_RECV_BUF_SIZE + CS_SOCKET_HTTP_HEADER_SIZE];
};