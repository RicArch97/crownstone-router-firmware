/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_Socket.h"
#include "cs_ReturnTypes.h"
#include "cs_PacketHandling.h"

#include <zephyr/kernel.h>

#include <stdbool.h>
#include <stdint.h>

#define CS_WEBSOCKET_THREAD_PRIORITY   K_PRIO_PREEMPT(5)
#define CS_WEBSOCKET_THREAD_STACK_SIZE 4096

#define CS_WEBSOCKET_HTTP_HEADER_SIZE  30
#define CS_WEBSOCKET_RECV_TIMEOUT      3000
#define CS_WEBSOCKET_RECV_RETRY_TIMOUT 50

#define CS_WEBSOCKET_CONNECTED_EVENT 0x001

class WebSocket : public Socket
{
      public:
	WebSocket() = default;
	/**
	 * @brief WebSocket constructor for data packaging and handling.
	 *
	 * @param handler PacketHandler instance
	 */
	WebSocket(PacketHandler *handler) : _pkt_handler(handler){};

	cs_err_t init(cs_socket_opts *opts);
	cs_err_t connect(const char *url);
	cs_err_t close();

	static void sendMessage(void *cls, uint8_t *msg, int msg_len);

	/** ID of the websocket */
	int _websock_id = -1;

	/** PacketHanler instance to handle packets */
	PacketHandler *_pkt_handler = NULL;

	/** Structure containing websocket receive thread information */
	k_thread _ws_recv_tid;
	/** Structure containing websocket send thread information */
	k_thread _ws_send_tid;
	/** Mailbox structure for passing data to the websocket handler thread */
	k_mbox _ws_mbox;
	/** Event structure used for an event when websocket is connected */
	k_event _evt_ws_connected;

	/** Receive buffer of 256 bytes for storing data received from the websocket */
	uint8_t _websocket_recv_buf[CS_PACKET_BUF_SIZE];
	/** Temp receive buffer with extra space for HTTP headers, for the HTTP handshake */
	uint8_t _websocket_recv_temp_buf[CS_PACKET_BUF_SIZE + CS_WEBSOCKET_HTTP_HEADER_SIZE];

      private:
	/** Initialized flag */
	bool _initialized = false;

	/** Pointer to the websocket receive thread structure */
	k_tid_t _ws_recv_thread = NULL;
	/** Pointer to the websocket send thread structure */
	k_tid_t _ws_send_thread = NULL;
};