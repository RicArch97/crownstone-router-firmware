/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include "socket/cs_Socket.h"
#include "socket/cs_CaCertificate.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Socket, LOG_LEVEL_INF);

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/tls_credentials.h>

#include <stdio.h>

/**
 * @brief Initialize socket.
 *
 * @param opts Instance of @ref cs_socket_opts.
 *
 * @return CS_OK if the initialization is successful.
 */
cs_err_t Socket::initSocket(struct cs_socket_opts *opts)
{
	if (_initialized) {
		LOG_ERR("Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	// register CA certificate when TLS should be used
	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		if (tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				       ca_certificate, sizeof(ca_certificate)) < 0) {
			LOG_ERR("Failed to register CA certificate");
			return CS_ERR_SOCKET_CA_CERT_REGISTER_FAILED;
		}
	}

	if (opts->host_mode == CS_SOCKET_HOST_DOMAIN) {
		struct zsock_addrinfo hints;
		hints.ai_family = opts->ip_ver == CS_SOCKET_IPV6 ? AF_INET6 : AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// 2 byte integer (65k, max 5 characters) to string
		char port_str[6]; // uint16 + terminator
		sprintf(port_str, "%u", opts->domain->port);

		// resolve host using DNS
		struct zsock_addrinfo *res;
		if (zsock_getaddrinfo(opts->domain->domain_name, port_str, &hints, &res) != 0) {
			LOG_ERR("Unable to resolve host address");
			return CS_ERR_SOCKET_UNABLE_TO_RESOLVE_HOST;
		}
		_addr = res->ai_addr;
		_addr_len = res->ai_addrlen;
		_host_name = opts->domain->domain_name;

		// create socket, using TLS when specified
		if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
			_sock_id = zsock_socket(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2);
		} else {
			_sock_id = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		}
	} else if (opts->host_mode == CS_SOCKET_HOST_ADDR) {
		if (opts->ip_ver == CS_SOCKET_IPV6) {
			struct sockaddr_in6 addr6;
			addr6.sin6_family = AF_INET6;
			addr6.sin6_port = htons(opts->addr->port);
			// convert ascii addr to internal representation
			zsock_inet_pton(AF_INET6, opts->addr->peer_addr, &addr6.sin6_addr);
			_addr = (struct sockaddr *)&addr6;
			_addr_len = sizeof(addr6);
		} else {
			struct sockaddr_in addr4;
			addr4.sin_family = AF_INET;
			addr4.sin_port = htons(opts->addr->port);
			// convert ascii addr to internal representation
			zsock_inet_pton(AF_INET, opts->addr->peer_addr, &addr4.sin_addr);
			_addr = (struct sockaddr *)&addr4;
			_addr_len = sizeof(addr4);
		}
		_host_name = opts->addr->host_name;

		// create socket, using TLS when specified
		if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
			_sock_id = zsock_socket(_addr->sa_family, SOCK_STREAM, IPPROTO_TLS_1_2);
		} else {
			_sock_id = zsock_socket(_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
		}
	} else {
		LOG_ERR("Invalid mode provided");
		return CS_ERR_SOCKET_INVALID_MODE;
	}

	if (_sock_id < 0) {
		LOG_ERR("Failed to create socket");
		return CS_ERR_SOCKET_CREATION_FAILED;
	}

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		if (zsock_setsockopt(_sock_id, SOL_TLS, TLS_HOSTNAME, _host_name,
				     strlen(_host_name)) < 0) {
			LOG_ERR("Failed to set TLS hostname");
			return CS_ERR_SOCKET_SET_TLS_HOSTNAME_FAILED;
		}

		sec_tag_t sec_tag_opt[] = {
			CA_CERTIFICATE_TAG,
		};
		// set TLS tags, in this case only certificate
		if (zsock_setsockopt(_sock_id, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt,
				     sizeof(sec_tag_opt)) < 0) {
			LOG_ERR("Failed to set TLS_SEC_TAG_LIST option");
			return CS_ERR_SOCKET_SET_TLS_TAG_LIST_FAILED;
		}
	}

	_initialized = true;

	return CS_OK;
}

/**
 * @brief Close socket.
 *
 * @return CS_OK if the socket was closed.
 */
cs_err_t Socket::closeSocket()
{
	if (!_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (_sock_id >= 0) {
		zsock_close(_sock_id);
		_sock_id = -1;
	}

	return CS_OK;
}