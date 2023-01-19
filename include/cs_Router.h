/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 1 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <zephyr/kernel.h>

#include <stdint.h>

#define CS_PROTOCOL_VERSION	 1
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
enum cs_router_uart_packet_type : uint8_t {
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
enum cs_router_generic_packet_type : uint8_t {
	CS_PACKET_TYPE_CONTROL,
	CS_PACKET_TYPE_RESULT,
	CS_PACKET_TYPE_DATA
};

/**
 * @brief Frame struct for a control packet.
 *
 * @param command_type Type of the command
 * @param src_id The id of the source device, used to send result packet back
 * @param dest_id The id of the destination device that should receive this command
 * @param length Length of the payload
 * @param payload Payload according to the command type, depends on length
 */
struct cs_router_control_packet {
	uint8_t command_type;
	uint8_t src_id;
	uint8_t dest_id;
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
 * @param source_id The id of the source where the data comes from
 * @param length Length of the data
 * @param payload Payload according to the data type, depends on length
 */
struct cs_router_data_packet {
	uint8_t source_id;
	uint16_t length;
	uint8_t *payload;
} __packed;

/**
 * @brief Control & result packet command types.
 */
enum cs_router_command_type : uint8_t {
	CS_COMMAND_TYPE_SET_CONFIG,
	CS_COMMAND_TYPE_GET_CONFIG,
	CS_COMMAND_TYPE_WIFI_CONNECT,
	CS_COMMAND_TYPE_WIFI_DISCONNECT,
	CS_COMMAND_TYPE_RESET,
	CS_COMMAND_TYPE_FACTORY_RESET,
	CS_COMMAND_TYPE_SWITCH
};

/**
 * @brief Result packet result types.
 */
enum cs_router_result_code : uint8_t {
	CS_RESULT_TYPE_SUCCES,
	CS_RESULT_TYPE_WAIT_FOR_SUCCESS,
	CS_RESULT_TYPE_SUCCESS_NO_CHANGE,
	CS_RESULT_TYPE_BUFFER_TOO_SMALL = (1 << 3),
	CS_RESULT_TYPE_WRONG_PAYLOAD_LENGTH = (1 << 4),
	CS_RESULT_TYPE_UNKNOWN_TYPE,
	CS_RESULT_TYPE_TIMEOUT,
	CS_RESULT_TYPE_CANCELED,
	CS_RESULT_TYPE_PROTOCOL_UNSUPPORTED,
	CS_RESULT_TYPE_MISMATCH,
	CS_RESUKT_TYPE_NOT_IMPLEMENTED = (1 << 5),
	CS_RESULT_TYPE_NOT_INITIALIZED,
	CS_RESULT_TYPE_READ_FAILED,
	CS_RESULT_TYPE_EVENT_UNHANDLED,
	CS_RESULT_TYPE_UNSPECIFIED = (1 << 7)
};

/**
 * @brief Instance ids.
 */
enum cs_router_instance_id : uint8_t {
	CS_INSTANCE_ID_ESP32, // the controller where this firmware runs, for configuration commands
	CS_INSTANCE_ID_UART_RS485, // e.g. solar panel / heatpump / charging station
	CS_INSTANCE_ID_UART_RS232, // e.g. dutch smart meter
	CS_INSTANCE_ID_UART_CM4,   // raspberry pi computer module 4 where application code runs
	CS_INSTANCE_ID_CLOUD,	   // cloud server where application code runs
	CS_INSTANCE_ID_BLE_CROWNSTONE_MESH,   // crownstone's BLE mesh
	CS_INSTANCE_ID_BLE_CROWNSTONE_CLIENT, // 1 to 1 BLE connection to a crownstone
	CS_INSTANCE_ID_AMOUNT		      // amount of instance id's available
};

/**
 * @brief Frame struct for a set config packet.
 *
 * @param config_type Type of configuration
 * @param config_id Identifier for the configuration to set
 * @param persistence_mode Type of persistence mode
 * @param payload Payload according to the config type
 * @param reserved Reserved for future use
 */
struct cs_router_set_config_packet {
	uint8_t config_type;
	uint8_t config_id;
	uint8_t persistence_mode;
	uint8_t *payload;
	uint8_t reserved;
} __packed;

/**
 * @brief Frame struct for a get config packet.
 *
 * @param config_type Type of configuration
 * @param config_id Identifier for the configuration to get
 * @param persistence_mode Type of persistence mode
 * @param reserved Reserved for future use
 */
struct cs_router_get_config_packet {
	uint8_t config_type;
	uint8_t config_id;
	uint8_t persistence_mode;
	uint8_t reserved;
} __packed;

/**
 * @brief Frame struct for a set config result packet.
 *
 * @param config_type Type of configuration that was set
 * @param config_id Identifier for the configuration that was set
 * @param persistence_mode Type of persistence mode used
 * @param reserved Reserved for future use
 */
struct cs_router_set_config_result_packet {
	uint8_t config_type;
	uint8_t config_id;
	uint8_t persistence_mode;
	uint8_t reserved;
} __packed;

/**
 * @brief Frame struct for a get config result packet.
 *
 * @param config_type Type of configuration that was requested
 * @param config_id Identifier for the configuration that was requested
 * @param persistence_mode Type of persistence mode requested
 * @param payload Payload according to the config type, containing the state
 * @param reserved Reserved for future use
 */
struct cs_router_get_config_result_packet {
	uint8_t config_type;
	uint8_t config_id;
	uint8_t persistence_mode;
	uint8_t *payload;
	uint8_t reserved;
} __packed;

/**
 * @brief Set config packet configuration types.
 */
enum cs_router_config_type : uint8_t {
	CS_CONFIG_TYPE_WIFI_SSID,     // max 32 bytes
	CS_CONFIG_TYPE_WIFI_PSK,      // max 64 bytes
	CS_CONFIG_TYPE_UART_BAUDRATE, // uint32
};

/**
 * @brief Set config persistence modes.
 */
enum cs_router_set_config_persistence_mode : uint8_t {
	CS_SET_CONFIG_PERSISTENCE_MODE_TEMPORARY,
	CS_SET_CONFIG_PERSISTENCE_MODE_STORED
};

/**
 * @brief Get config persistence modes.
 */
enum cs_router_get_config_persistence_mode : uint8_t {
	CS_GET_CONFIG_PERSISTENCE_MODE_CURRENT,
	CS_GET_CONFIG_PERSISTENCE_MODE_STORED,
	CS_GET_CONFIG_PERSISTENCE_MODE_FIRMWARE_DEFAULT
};

/**
 * @brief Frame struct for a switch command packet.
 *
 * @param switch_value Value from 0 to 100, can be analog to support dimming.
 * when using digital, 0 means off and 100 means on.
 */
struct cs_router_switch_command_packet {
	uint8_t switch_value;
} __packed;