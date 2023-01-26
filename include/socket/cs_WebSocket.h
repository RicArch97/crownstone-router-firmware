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

#define CS_WEBSOCKET_THREAD_PRIORITY   K_PRIO_PREEMPT(7)
#define CS_WEBSOCKET_THREAD_STACK_SIZE 4096

#define CS_WEBSOCKET_HTTP_HEADER_SIZE  30
#define CS_WEBSOCKET_RECV_RETRY_TIMOUT 50
#define CS_WEBSOCKET_URL_MAX_LEN       32

#define CS_WEBSOCKET_CONNECTED_EVENT 0x001

class WebSocket : public Socket
{
      public:
	WebSocket() = default;
	/**
	 * @brief WebSocket constructor for data packaging and handling.
	 *
	 * @param src_id Identifier for the websocket, used for incoming packets
	 * @param handler PacketHandler instance
	 */
	WebSocket(cs_router_instance_id src_id, PacketHandler *handler)
		: _src_id(src_id), _pkt_handler(handler){};

	cs_ret_code_t connect(const char *url);
	cs_ret_code_t close();

	static void sendMessage(void *inst, uint8_t *msg, int msg_len);

	/** ID of the websocket */
	int _websock_id = -1;

	/** Websocket source id, to identify as incoming data handler */
	cs_router_instance_id _src_id = CS_INSTANCE_ID_UNKNOWN;
	/** PacketHanler instance to handle packets */
	PacketHandler *_pkt_handler = NULL;

	/** Structure containing websocket receive thread information */
	k_thread _ws_tid;
	/** Event structure used for an event when websocket is connected */
	k_event _ws_evts;

	/** Receive buffer of 256 bytes for storing data received from the websocket */
	uint8_t _ws_recv_buf[CS_PACKET_BUF_SIZE];
	/** Temp receive buffer with extra space for HTTP headers, for the HTTP handshake */
	uint8_t _ws_recv_tmp_buf[CS_PACKET_BUF_SIZE + CS_WEBSOCKET_HTTP_HEADER_SIZE];
};