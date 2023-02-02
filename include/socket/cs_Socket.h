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

#define DOMAIN_NAME_MAX_LEN 64

/**
 * @brief Socket IP versions.
 */
enum cs_socket_ip {
	CS_SOCKET_IPV4,
	CS_SOCKET_IPV6
};

class Socket
{
      public:
	Socket() = default;
	~Socket();
	cs_ret_code_t init(const char *domain_name, uint16_t port);
	cs_ret_code_t init(const char *peer_addr, cs_socket_ip ip_ver, uint16_t port);
	cs_ret_code_t close();

	/** Initialized flag */
	bool _initialized = false;

	/** ID of the socket */
	int _sock_id = -1;

      protected:
	/** Structure containing address info resolved by DNS */
	zsock_addrinfo *_res = NULL;
	/** Generic structure with address information */
	sockaddr _addr;
	/** Length of the address */
	int _addr_len = 0;
	/** Host address of domain */
	char _host[DOMAIN_NAME_MAX_LEN];
};