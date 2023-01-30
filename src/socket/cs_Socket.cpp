/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include "socket/cs_Socket.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Socket, LOG_LEVEL_INF);

#include <stdio.h>

/**
 * @brief Initialize socket for a domain name.
 * Uses DNS to determine address.
 *
 * @param domain_name Name of the domain. For example: crownstone.rocks, max 64 characters
 * @param port Port that the connection should be opened on
 *
 * @return CS_OK if the initialization is successful.
 */
cs_ret_code_t Socket::init(const char *domain_name, uint16_t port)
{
	if (_initialized) {
		LOG_ERR("%s", "Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	zsock_addrinfo hints;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;

	// 2 byte integer (65k, max 5 characters) to string
	char port_str[6]; // uint16 + terminator
	sprintf(port_str, "%u", port);

	// resolve host using DNS
	if (zsock_getaddrinfo(domain_name, port_str, &hints, &_res) != 0) {
		LOG_ERR("%s", "Unable to resolve host address");
		return CS_ERR_SOCKET_UNABLE_TO_RESOLVE_HOST;
	}

	_addr = *_res->ai_addr;
	strncpy(_host, domain_name, sizeof(_host));

	_sock_id = zsock_socket(_res->ai_family, _res->ai_socktype, _res->ai_protocol);
	if (_sock_id < 0) {
		LOG_ERR("%s", "Failed to create socket for domain");
		return CS_ERR_SOCKET_CREATION_FAILED;
	}

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Initialize socket for a peer address.
 *
 * @param peer_name Address of the peer, IPv4 or IPv6
 * @param ip_ver Which IP version is used, one of @ref cs_socket_ip
 * @param port Port that the connection should be opened on
 *
 * @return CS_OK if the initialization is successful.
 */
cs_ret_code_t Socket::init(const char *peer_addr, cs_socket_ip ip_ver, uint16_t port)
{
	if (ip_ver == CS_SOCKET_IPV6) {
		sockaddr_in6 *addr6 = net_sin6(&_addr);
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons(port);

		zsock_inet_pton(AF_INET6, peer_addr, &addr6->sin6_addr);

		_addr_len = sizeof(*addr6);
	} else {
		sockaddr_in *addr4 = net_sin(&_addr);
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons(port);

		zsock_inet_pton(AF_INET, peer_addr, &addr4->sin_addr);

		_addr_len = sizeof(*addr4);
	}

	strncpy(_host, peer_addr, sizeof(_host));

	_sock_id = zsock_socket(_addr.sa_family, SOCK_STREAM, IPPROTO_TCP);
	if (_sock_id < 0) {
		LOG_ERR("%s", "Failed to create socket for domain");
		return CS_ERR_SOCKET_CREATION_FAILED;
	}

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Close socket.
 *
 * @return CS_OK if the socket was closed.
 */
cs_ret_code_t Socket::close()
{
	if (!_initialized) {
		LOG_ERR("%s", "Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_sock_id >= 0) {
		zsock_close(_sock_id);
		_sock_id = -1;
	}

	return CS_OK;
}

/**
 * @brief Free all heap allocated memory.
 */
Socket::~Socket()
{
	zsock_freeaddrinfo(_res);
}