/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 6 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Socket hosts.
 */
enum cs_socket_host {
	CS_SOCKET_HOST_DOMAIN,
	CS_SOCKET_HOST_ADDR
};

/**
 * @brief Socket IP versions.
 */
enum cs_socket_ip_version {
	CS_SOCKET_IPV4,
	CS_SOCKET_IPV6
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
 * @param port Peer port
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
 * @param host_mode Socket mode. One of @ref cs_socket_modes
 * @param ip_version IP version. One of @ref cs_socket_ip_version
 * @param domain Instance of @ref cs_socket_host_domain, set this when setting CS_SOCKET_HOST_DOMAIN
 * as host mode
 * @param addr Instance of @ref cs_socket_host_addr, set this when setting CS_SOCKET_HOST_ADDR as
 * host mode
 */
struct cs_socket_opts {
	enum cs_socket_host host_mode;
	enum cs_socket_ip_version ip_ver;
	union {
		struct cs_socket_host_domain *domain;
		struct cs_socket_host_addr *addr;
	};
};

class Socket
{
      public:
	cs_ret_code_t init(struct cs_socket_opts *opts);
	cs_ret_code_t close();

	/** ID of the socket */
	int _sock_id = -1;

	/** Provides address hints to DNS resolver */
	zsock_addrinfo _hints;
	/** Address structure for IPv4 */
	sockaddr_in _addr4;
	/** Address structure for IPv6 */
	sockaddr_in6 _addr6;

      protected:
	/** Generic structure with address information, pointer to either addr4 or addr6 */
	sockaddr *_addr = NULL;
	/** Length of the address */
	int _addr_len = 0;
	/** Name of the host */
	const char *_host_name = NULL;

      private:
	/** Initialized flag */
	bool _initialized = false;
};