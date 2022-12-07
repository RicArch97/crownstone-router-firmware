/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 5 Dec., 2022
 * License: Apache License 2.0
 */

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Socket, LOG_LEVEL_INF);

#include "cs_ReturnTypes.h"
#include "cs_Router.h"
#include "socket/cs_WebSocket.h"

#include <zephyr/net/net_ip.h>
#include <zephyr/net/socket.h>

#include <zephyr/net/tls_credentials.h>
#include "socket/cs_CaCertificate.h"

/**
 * @brief Initialize websocket.
 *
 * @param opts Instance of @ref cs_socket_opts. port and one of host_url
 * or peer_addr should be provided. hostname is required for TLS and peer_addr.
 *
 * @return CS_OK if the initialization is successful.
 */
cs_err_t WebSocket::init(struct cs_socket_opts *opts)
{
	if (_is_initialized) {
		LOG_ERR("Already initialized");
		return CS_ERR_ALREADY_INITIALIZED;
	}

	// register CA certificate when TLS should be used
	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		if (tls_credential_add(CA_CERTIFICATE_TAG, TLS_CREDENTIAL_CA_CERTIFICATE,
				       ca_certificate, sizeof(ca_certificate)) < 0) {
			LOG_ERR("Failed to register public certificate: %d", err);
			return CS_ERR_SOCKET_CA_CERT_REGISTER_FAILED;
		}
	}

	if (opts->host_mode == CS_SOCKET_HOST_DOMAIN) {
		static struct zsock_addrinfo hints;
		hints.ai_family = opts->use_ipv6 ? AF_INET6 : AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		hints.ai_protocol = IPPROTO_TCP;

		// 2 byte integer to string
		char port_str[3]; // uint16 + terminator
		snprintk(port_str, sizeof(port_str), "%s", opts->domain.port);

		// resolve host using DNS
		struct zsock_addrinfo *res;
		if (zsock_getaddrinfo(opts->domain.domain_name, port_str, &hints, &res) != 0) {
			LOG_ERR("Unable to resolve host address");
			return CS_ERR_SOCKET_UNABLE_TO_RESOLVE_HOST;
		}
		_addr = res->ai_addr;
		_addr_len = res->ai_addrlen;
		_host_name = opts->domain.domain_name;

		// create socket, using TLS when specified
		if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
			_sock_nr = zsock_socket(res->ai_family, res->ai_socktype, IPPROTO_TLS_1_2);
		} else {
			_sock_nr = zsock_socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		}
	} else if (opts->host_mode == CS_SOCKET_HOST_ADDR) {
		if (opts->use_ipv6) {
			struct sockaddr_in6 addr6;
			addr6.sin6_family = AF_INET6;
			addr6.sin6_port = htons(opts->addr.port);
			// convert ascii addr to internal representation
			zsock_inet_pton(AF_INET6, opts->addr.peer_addr, &addr6.sin6_addr);
			_addr = (struct sockaddr *)&addr6;
			_addr_len = sizeof(addr6);
		} else {
			struct sockaddr_in addr4;
			addr4.sin_family = AF_INET;
			addr4.sin_port = htons(opts->addr.port);
			// convert ascii addr to internal representation
			zsock_inet_pton(AF_INET, opts->addr.peer_addr, &addr4.sin_addr);
			_addr = (struct sockaddr *)&addr4;
			_addr_len = sizeof(addr4);
		}
		_host_name = opts->addr.host_name;

		// create socket, using TLS when specified
		if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
			_sock_nr = zsock_socket(_addr->sa_family, SOCK_STREAM, IPPROTO_TLS_1_2);

		} else {
			_sock_nr = zsock_socket(_addr->sa_family, SOCK_STREAM, IPPROTO_TCP);
		}
	} else {
		LOG_ERR("Invalid mode provided");
		return CS_ERR_SOCKET_INVALID_MODE;
	}

	if (_sock_nr < 0) {
		LOG_ERR("Failed to create socket");
		return CS_ERR_SOCKET_CREATION_FAILED;
	}

	if (IS_ENABLED(CONFIG_NET_SOCKETS_SOCKOPT_TLS)) {
		if (zsock_setsockopt(_sock_nr, SOL_TLS, TLS_HOSTNAME, _host_name,
				     strlen(_host_name)) < 0) {
			LOG_ERR("Failed to set TLS hostname");
			return CS_ERR_SOCKET_SET_TLS_HOSTNAME_FAILED;
		}

		sec_tag_t sec_tag_opt[] = {
			CA_CERTIFICATE_TAG,
		};
		// set TLS tags, in this case only certificate
		if (zsock_setsockopt(_sock_nr, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_opt,
				     sizeof(sec_tag_opt)) < 0) {
			LOG_ERR("Failed to set TLS_SEC_TAG_LIST option");
			return CS_ERR_SOCKET_SET_TLS_TAG_LIST_FAILED;
		}
	}

	return CS_OK;
}

/**
 * @brief Connect the socket to the network.
 *
 * @return CS_OK if connection is successful.
 */
cs_err_t WebSocket::connect()
{
	if (!_is_initialized) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (zsock_connect(_sock_nr, _addr, _addr_len) < 0) {
		return CS_FAIL;
	}

	return CS_OK;
}

/**
 * @brief Close socket.
 */
cs_err_t WebSocket::close()
{
	if (!_is_initialized || _sock_nr < 0) {
		LOG_ERR("Not initialized");
		return CS_ERR_NOT_INITIALIZED;
	}

	if (zsock_close(_sock_nr) < 0) {
		LOG_ERR("Failed to close socket");
		return CS_ERR_SOCKET_CLOSE_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Send a HTTP GET request over the socket.
 *
 * @param endpoint Endpoint / url for the request
 *
 * @return CS_OK if the request was done succesfully.
 */
cs_err_t WebSocket::sendHttpGetRequest(const char *endpoint)
{
	struct http_request get_req = {0};

	get_req.method = HTTP_GET;
	get_req.url = strcat("/", endpoint);
	get_req.host = _host_name;
	get_req.protocol = HTTP_PROTOCOL_VER;
	get_req.recv_buf = _socket_recv_buf;
	get_req.recv_buf_len = CS_SOCKET_RECV_BUFFER_SIZE;

	if (http_client_req(_sock_nr, &get_req, CS_SOCKET_RECV_TIMEOUT, this) < 0) {
		LOG_ERR("Failed to send http GET request");
		return CS_ERR_SOCKET_HTTP_GET_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Send a HTTP POST request over the socket.
 * 
 * @param endpoint Endpoint / url of the request
 * @param payload Payload that should be send to the server
*/
cs_err_t WebSocket::sendHttpPostRequest(const char *endpoint, const char *payload)
{
	struct http_request post_req = {0};

	post_req.method = HTTP_POST;
	post_req.url = strcat("/", endpoint);
	post_req.host = _host_name;
	post_req.protocol = HTTP_PROTOCOL_VER;
	post_req.payload = payload;
	post_req.payload_len = strlen(payload);
	post_req.recv_buf = _socket_recv_buf;
	post_req.recv_buf_len = CS_SOCKET_RECV_BUFFER_SIZE;

	if (http_client_req(_sock_nr, &post_req, CS_SOCKET_RECV_TIMEOUT, this) < 0) {
		LOG_ERR("Failed to send http POST request");
		return CS_ERR_SOCKET_HTTP_GET_FAILED;
	}

	return CS_OK;
}