/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 18 Nov., 2022
 * License: Apache License 2.0
*/

#pragma once

typedef int cs_err_t;

#define CS_OK   0
#define CS_FAIL -1

#define CS_ERR_ALREADY_INITIALIZED              0x101
#define CS_ERR_NOT_INITIALIZED                  0x102
#define CS_ERR_DEVICE_NOT_READY                 0x103
#define CS_ERR_INTERFACE_NOT_AVAILABLE          0x104
#define CS_ERR_NOT_SUPPORTED                    0x105
#define CS_ERR_NOT_ENABLED                      0x106

#define CS_ERR_WIFI_CONNECT_REQUEST_FAILED      0x201
#define CS_ERR_WIFI_SCAN_REQUEST_FAILED         0x202
#define CS_ERR_WIFI_SCAN_RESULT_TIMEOUT         0x203
#define CS_ERR_WIFI_DISCONNECT_REQUEST_FAILED   0x204
#define CS_ERR_WIFI_NOT_CONNECTED               0x205

#define CS_ERR_UART_CONFIG_INVALID              0x301
#define CS_ERR_UART_CONFIG_FAILED               0x302
#define CS_ERR_UART_RX_FAILED                   0x303

#define CS_ERR_SOCKET_UNABLE_TO_RESOLVE_HOST    0x401
#define CS_ERR_SOCKET_CREATION_FAILED           0x402
#define CS_ERR_SOCKET_INVALID_MODE              0x403
#define CS_ERR_SOCKET_CA_CERT_REGISTER_FAILED   0x404
#define CS_ERR_SOCKET_SET_TLS_TAG_LIST_FAILED   0x405
#define CS_ERR_SOCKET_SET_TLS_HOSTNAME_FAILED   0x406