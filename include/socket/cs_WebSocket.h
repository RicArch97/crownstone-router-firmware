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

#include <stdbool.h>
#include <stdint.h>

#define CS_WEBSOCKET_THREAD_PRIORITY   K_PRIO_PREEMPT(5)
#define CS_WEBSOCKET_THREAD_STACK_SIZE 4096
#define CS_WEBSOCKET_THREAD_SLEEP      100

#define CS_WEBSOCKET_RECV_BUF_SIZE     512
#define CS_WEBSOCKET_HTTP_HEADER_SIZE  30
#define CS_WEBSOCKET_RECV_TIMEOUT      3000
#define CS_WEBSOCKET_RECV_RETRY_TIMOUT 50

#define CS_WEBSOCKET_MBOX_BUF_SIZE 512

class WebSocket : public Socket
{
public:
	WebSocket() = default;

	cs_err_t init(struct cs_socket_opts *opts);
	cs_err_t connect(const char *url);
	cs_err_t sendMessage(uint8_t *message, size_t len, struct k_sem *sem);
	cs_err_t close();

	/** ID of the websocket */
	int _websock_id = -1;

	/** Structure containing websocket thread information */
	struct k_thread _ws_tid;
	/** Mailbox structure for passing data to the websocket handler thread */
	struct k_mbox _ws_mbox;

	/** Receive buffer of 512 bytes for storing data received from the websocket */
	uint8_t _websocket_recv_buf[CS_WEBSOCKET_RECV_BUF_SIZE];
	/** Temp receive buffer with extra space for HTTP headers, for the HTTP handshake */
	uint8_t _websocket_recv_temp_buf[CS_WEBSOCKET_RECV_BUF_SIZE +
					 CS_WEBSOCKET_HTTP_HEADER_SIZE];

private:
	/** Initialized flag */
	bool _initialized = false;

	/** Pointer to the websocket thread structure */
	k_tid_t _ws_thread = NULL;
};