/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 1 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include "cs_ReturnTypes.h"

#include <stdint.h>

#define CS_PROTOCOL_VERSION 1
#define CS_UART_PROTOCOL_VERSION 1

/**
 * @brief Frame struct for an UART Crownstone router communication packet.
 * Used for local communication.
 *
 * @param start_token Start token for a message
 * @param length Length of all data beyond this field, including CRC
 * @param protocol_version Uart protocol version
 * @param type Frame type
 * @param payload Payload containing a Crownstone router packet
 * @param crc CRC16 of everything after length
 */
struct cs_router_uart_packet {
	uint8_t start_token;
	uint16_t length;
	uint8_t protocol_version;
	uint8_t type;
	uint8_t *payload;
	uint16_t crc;
} __packed;

/**
 * @brief Crownstone router packet types.
 */
enum cs_router_uart_packet_type {
	CS_PACKET_TYPE_GENERIC,
	/** CS_PACKET_TYPE_GENERIC_ENCRYPTED */
};

/**
 * @brief Frame struct for a generic Crownstone router communication packet.
 * Used for cloud communication.
 *
 * @param protocol_version Protocol version
 * @param type Frame type
 * @param length Length of the payload
 * @param payload Payload containing an other packet
 */
struct cs_router_generic_packet {
	uint8_t protocol_version;
	uint8_t type;
	uint16_t length;
	uint8_t *payload;
} __packed;

/**
 * @brief Generic packet types.
 */
enum cs_router_generic_packet_type {
	CS_PACKET_TYPE_CONTROL,
	CS_PACKET_TYPE_RESULT,
	CS_PACKET_TYPE_DATA
};

/**
 * @brief Frame struct for a control packet.
 *
 * @param command_type Type of the command
 * @param length Length of the payload
 * @param payload Payload according to the command type, depends on length
 */
struct cs_router_control_packet {
	uint8_t command_type;
	uint16_t length;
	uint8_t *payload;
} __packed;

/**
 * @brief Frame struct for a result packet.
 *
 * @param command_type Type of the command of which this packet is the result
 * @param result_code The result code
 * @param length Length of the payload
 * @param payload Payload according to the command type, depends on length
 */
struct cs_router_result_packet {
	uint8_t command_type;
	uint8_t result_code;
	uint16_t length;
	uint8_t *payload;
} __packed;

/**
 * @brief Frame struct for a data packet.
 *
 * @param source_type The type of source
 * @param source_id The id of the source where the data comes from
 * @param length Length of the data
 * @param payload Payload according to the data type, depends on length
 */
struct cs_router_data_packet {
	uint8_t source_type;
	uint8_t source_id;
	uint16_t length;
	uint8_t *payload;
} __packed;

/**
 * @brief Control & result packet command types.
 */
enum cs_router_command_type {
	CS_COMMAND_TYPE_SET_CONFIG,
	CS_COMMAND_TYPE_GET_CONFIG,
	CS_COMMAND_TYPE_WIFI_CONNECT,
	CS_COMMAND_TYPE_WIFI_DISCONNECT,
	CS_COMMAND_TYPE_RESET,
	CS_COMMAND_TYPE_FACTORY_RESET
};

/**
 * @brief Result packet result types.
 */
enum cs_router_result_code {
	CS_RESULT_TYPE_SUCCES,
	CS_RESULT_TYPE_WAIT_FOR_SUCCESS,
	CS_RESULT_TYPE_SUCCESS_NO_CHANGE,
	CS_RESULT_TYPE_BUFFER_UNASSIGNED = (1 << 3),
	CS_RESULT_TYPE_BUFFER_TOO_SMALL,
	CS_RESULT_TYPE_WRONG_PAYLOAD_LENGTH = (1 << 4),
	CS_RESULT_TYPE_UNKNOWN_TYPE,
	CS_RESULT_TYPE_NO_SPACE,
	CS_RESULT_TYPE_BUSY,
	CS_RESULT_TYPE_TIMEOUT,
	CS_RESULT_TYPE_CANCELED,
	CS_RESULT_TYPE_PROTOCOL_UNSUPPORTED,
	CS_RESULT_TYPE_MISMATCH,
	CS_RESUKT_TYPE_NOT_IMPLEMENTED = (1 << 5),
	CS_RESULT_TYPE_NOT_INITIALIZED,
	CS_RESULT_TYPE_READ_FAILED,
	CS_RESULT_TYPE_UNSPECIFIED = (1 << 8)
};

/**
 * @brief Data packet data types.
 */
enum cs_router_source_type {
	CS_SOURCE_TYPE_UART,
	CS_SOURCE_TYPE_BLE,
	CS_SOURCE_TYPE_CLOUD
};

/**
 * @brief Data packet uart source id's.
 */
enum cs_router_source_uart_id {
	CS_SOURCE_ID_UART_RS485, // e.g. solar panel / heatpump / charging station
	CS_SOURCE_ID_UART_RS232, // e.g. dutch smart meter
	CS_SOURCE_ID_UART_CM4    // raspberry pi computer module 4 where application code runs
};

/**
 * @brief Data packet cloud source id's.
 */
enum cs_router_source_cloud_id {
	CS_SOURCE_ID_CLOUD_SERVER // cloud server where application code runs
};

/**
 * @brief Data packet ble source id's.
 */
enum cs_router_source_ble_id {
	CS_SOURCE_ID_BLE_CROWNSTONE_MESH,
	CS_SOURCE_ID_BLE_CROWNSTONE_APP // possible for wifi detail exchange?
};

/**
 * @brief Frame struct for a set config packet.
 *
 * @param config_type Type of configuration
 * @param persistence_mode Whether the configuration should be stored
 * @param length Length of the payload
 * @param payload Payload according to the config type
 * @param reserved Reserved for future use
 */
struct cs_router_set_config_packet {
	uint8_t config_type;
	uint8_t persistence_mode;
	uint8_t length;
	uint8_t *payload;
	uint8_t reserved;
} __packed;

/**
 * @brief Frame struct for a get config packet.
 *
 * @param config_type Type of configuration
 * @param persistence_mode Whether the configuration should be stored
 * @param reserved Reserved for future use
 */
struct cs_router_get_config_packet {
	uint8_t config_type;
	uint8_t persistence_mode;
	uint8_t reserved;
} __packed;

/**
 * @brief Set config packet configuration types.
 */
enum cs_router_config_type {
	CS_CONFIG_TYPE_WIFI_SSID,     // max 32 bytes
	CS_CONFIG_TYPE_WIFI_PSK,      // max 64 bytes
	CS_CONFIG_TYPE_UART_BAUDRATE, // uint32
};

/**
 * @brief Set config & get config persistence modes.
 */
enum cs_router_config_persistence_mode {
	CS_CONFIG_PERSISTENCE_MODE_TEMPORARY,
	CS_CONFIG_PERSISTENCE_MODE_STORED
};