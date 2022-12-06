/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdbool.h>

#include "cs_ReturnTypes.h"

/**
 * @brief Socket modes.
*/
enum cs_socket_modes {
	CS_SOCKET_MODE_HTTP,
	CS_SOCKET_MODE_PEER
};

/**
 * @brief Socket mode http, for connecting to http hosts.
 * 
 * @param host_name Host url/name. Uses DNS to get the address info
 * Example: crownstone.rocks
 * @param port Host port. Usually 80 for non-TLS and 443 for TLS
*/
struct cs_socket_mode_http {
	const char *host_name;
	uint16_t port;
};

/**
 * @brief Socket mode peer, for connecting to given IP addresses (peers), usually local.
 * 
 * @param peer_addr Peer address. Example 192.0.2.2 or 2001:db8::2
 * @param port Peer port. Depends on own selection
 * @param host_name Hostname of the peer address, for example localhost. Used for TLS
*/
struct cs_socket_mode_peer {
	const char *peer_addr;
	uint16_t port;
	const char *host_name;
};

/**
 * @brief Socket options.
 *
 * @param socket_mode Socket mode. One of @ref cs_socket_modes
 * @param use_tls Whether TLS should be enabled for the socket, CA certificate is required
 * @param use_ipv6 Whether ipv6 should be used, instead of ipv4
 * @param http Instance of @ref cs_socket_mode_http, set with CS_SOCKET_MODE_HTTP
 * @param peer Instance of @ref cs_socket_mode_peer, set with CS_SOCKET_MODE_PEER
 */
struct cs_socket_opts {
	enum cs_socket_modes socket_mode;
	bool use_tls;
	bool use_ipv6;
	union {
		struct cs_socket_mode_http http;
		struct cs_socket_mode_peer peer;
	};
};

class WebSocket
{
public:
	WebSocket() {};

	cs_err_t init(struct cs_socket_opts *opts);
	cs_err_t connect();

private:
	bool _is_initialized = false;

	int _sock = -1;
  struct sockaddr *_addr = NULL;
	int _addr_len = 0;
};