/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdbool.h>

#include <zephyr/net/http/client.h>

#include "cs_ReturnTypes.h"

#if defined(CONFIG_NET_SOCKETS_SOCKOPT_TLS)
#define HTTP_PORT 443
#else
#define HTTP_PORT 80
#endif
#define HTTP_PROTOCOL_VER "HTTP/1.1"

#define CS_SOCKET_RECV_BUFFER_SIZE 512
#define CS_SOCKET_RECV_TIMEOUT	   3000

/**
 * @brief Socket hosts.
 */
enum cs_socket_host {
	CS_SOCKET_HOST_DOMAIN,
	CS_SOCKET_HOST_ADDR
};

/**
 * @brief Socket host mode domain, for connecting to domain names.
 *
 * @param domain_name domain name. Uses DNS to get the address info
 * Example: crownstone.rocks
 * @param port Host port. Usually 80 for non-TLS and 443 for TLS
 */
struct cs_socket_host_domain {
	const char *domain_name;
	uint16_t port;
};

/**
 * @brief Socket host mode addr, for connecting to given IP addresses (peers).
 *
 * @param peer_addr Peer address. Example 192.0.2.2 (ipv4) or 2001:db8::2 (ipv6)
 * @param port Peer port.
 * @param host_name Hostname of the peer address, for example localhost. Used for TLS
 */
struct cs_socket_host_addr {
	const char *peer_addr;
	uint16_t port;
	const char *host_name;
};

/**
 * @brief Socket options.
 *
 * @param socket_mode Socket mode. One of @ref cs_socket_modes
 * @param use_ipv6 Whether ipv6 should be used, instead of ipv4
 * @param http Instance of @ref cs_socket_mode_http, set with CS_SOCKET_MODE_HTTP
 * @param peer Instance of @ref cs_socket_mode_peer, set with CS_SOCKET_MODE_PEER
 */
struct cs_socket_opts {
	enum cs_socket_host host_mode;
	bool use_ipv6;
	union {
		struct cs_socket_host_domain domain;
		struct cs_socket_host_addr addr;
	};
};

class WebSocket
{
public:
	WebSocket() {};

	cs_err_t init(struct cs_socket_opts *opts);
	cs_err_t connect();
	cs_err_t close();
	cs_err_t sendHttpGetRequest(const char *endpoint);
	cs_err_t sendHttpPostRequest(const char *endpoint, const char *payload);

private:
	bool _is_initialized = false;

	int _sock_nr = -1;
	struct sockaddr *_addr = NULL;
	int _addr_len = 0;
	const char *_host_name = NULL;

	uint8_t _socket_recv_buf[CS_SOCKET_RECV_BUFFER_SIZE];
};