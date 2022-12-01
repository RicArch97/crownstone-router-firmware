/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 1 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#include "cs_ReturnTypes.h"

/**
 * @brief Frame struct for the Crownstone router communication protocol.
 */
struct cs_router_protocol {
	/** protocol version */
	struct cs_router_protocol_version version;
	/** frame type */
	struct cs_router_protocol_type type;
	/** length of the rest of the structure up until checksum */
	uint16_t length;
	/** where the data is coming from */
	uint16_t source_id;
};

/**
 * @brief Frame struct containing protocol version data.
 */
struct cs_router_protocol_version {
	/** increased for breaking changes */
	uint8_t major_version;
	/** increased when new types are added */
	uint8_t minor_version;
};

/**
 * @brief Frame struct containing frame type information.
 */
struct cs_router_protocol_type {
	/** main type */
	enum cs_router_protocol_type_main main_type;
	/** sub type */
	union sub_type {
		enum cs_router_protocol_type_command command_type;
		enum cs_router_protocol_type_config config_type;
		enum cs_router_protocol_type_result result_type;
	};
};

/**
 * @brief Frame main type.
 */
enum cs_router_protocol_type_main {
	CS_TYPE_COMMAND,
	CS_TYPE_CONFIG,
	CS_TYPE_RESULT,
	CS_TYPE_INCOMING_DATA
};

/**
 * @brief Frame sub type command.
 *
 * Command types hinting that a command should be executed on the router.
 */
enum cs_router_protocol_type_command {
	CS_TYPE_COMMAND_SETUP,
	CS_TYPE_COMMAND_RESET,
	CS_TYPE_COMMAND_GET_STATE,
	CS_TYPE_COMMAND_SET_STATE
};

/**
 * @brief Frame sub type config.
 *
 * Config options for the router itself which can be used
 * to override default configuration without editing the firmware.
 */
enum cs_router_protocol_type_config {
	CS_TYPE_CONFIG_WIFI_SSID,
	CS_TYPE_CONFIG_WIFI_PSK,
	CS_TYPE_CONFIG_UART_BAUDRATE,
	CS_TYPE_CONFIG_UART_PARITY,
	CS_TYPE_CONFIG_UART_STOP_BITS,
	CS_TYPE_CONFIG_UART_RX_BUFFER_SIZE,
	CS_TYPE_CONFIG_UART_RX_BUFFER_QUEUE_SIZE
};

/**
 * @brief Frame sub type result.
 */
enum cs_router_protocol_type_result {
	CS_TYPE_RESULT_SUCESS,
	CS_TYPE_RESULT_MISMATCH,
	CS_TYPE_RESULT_UNKNOWN_TYPE
};