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

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"

#define CS_WEBSOCKET_THREAD_PRIORITY   K_PRIO_PREEMPT(8)
#define CS_WEBSOCKET_THREAD_STACK_SIZE 1024
#define CS_WEBSOCKET_THREAD_SLEEP      100

#define CS_WEBSOCKET_RECV_BUF_SIZE     512
#define CS_WEBSOCKET_HTTP_HEADER_SIZE  30
#define CS_WEBSOCKET_RECV_TIMEOUT      3000
#define CS_WEBSOCKET_RECV_RETRY_TIMOUT 50

#define CS_WEBSOCKET_MBOX_BUF_SIZE 512

class WebSocket : Socket
{
public:
	cs_err_t init(struct cs_socket_opts *opts);
	cs_err_t connect(const char *url);
	void sendMessage(uint8_t *message, size_t len, struct k_sem *sem);
	cs_err_t close();

private:
	static cs_err_t handleWebsocketConnect(int sock, struct http_request *req, void *user_data);
	static void handleTransport(void *cls, void *unused1, void *unused2);

	bool _is_initialized = false;
	int _websock_id = -1;

	struct k_mbox _ws_mbox;
	k_tid_t _ws_thread = NULL;

	uint8_t _websocket_recv_buf[CS_WEBSOCKET_RECV_BUF_SIZE];
	uint8_t _websocket_recv_temp_buf[CS_WEBSOCKET_RECV_BUF_SIZE +
					 CS_WEBSOCKET_HTTP_HEADER_SIZE];
};