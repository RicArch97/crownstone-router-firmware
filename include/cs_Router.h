/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 1 Dec., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#include <zephyr/net/wifi.h>

#include "cs_ReturnTypes.h"

/**
 * @brief Frame struct for an UART Crownstone router communication packet.
 * Used for local communication.
 */
struct cs_router_uart_packet {
	/** Start token for a message */
	uint8_t start_token;
	/** Uart protocol version */
	uint8_t protocol_version;
	/** Length of all data beyond this field, including CRC */
	uint16_t length;
	/** Frame type */
	uint16_t type;
	/** Payload containing a Crownstone router packet */
	uint8_t *payload;
	/** CRC16 of the payload */
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
 */
struct cs_router_generic_packet {
	/** Protocol version */
	uint8_t protocol_version;
	/** Frame type */
	uint16_t type;
	/** Length of the payload */
	uint16_t length;
	/** Payload containing an other data structure */
	uint8_t *payload;
} __packed;

/**
 * @brief Generic packet types.
 */
enum cs_router_generic_packet_type {
	CS_PACKET_TYPE_CONTROL,
	CS_PACKET_TYPE_RESULT,
	CS_PACKET_TYPE_INCOMING_DATA
};

/**
 * @brief Frame struct for a control packet.
 */
struct cs_router_control_packet {
	/** Type of the command */
	uint16_t command_type;
	/** Length of the payload */
	uint16_t length;
	/** Payload according to the command type */
	uint8_t *payload;
	/** Reserved for future use */
	uint8_t reserved;
} __packed;

/**
 * @brief Control packet types.
 */
enum cs_router_control_packet_type {
	CS_PACKET_TYPE_CONTROL_CONFIG,
	CS_PACKET_TYPE_CONTROL_OPERATION,
	CS_PACKET_TYPE_CONTROL_GET_STATE,
	CS_PACKET_TYPE_CONTROL_RESET,
	CS_PACKET_TYPE_CONTROL_FACTORY_RESET
};

/**
 * @brief Frame struct for a config packet
 */
struct cs_router_config_packet {
	/** Type of configuration */
	uint16_t config_type;
	/** Whether the configuration should be stored */
	uint8_t persistence_mode;
	/** Length of the payload */
	uint16_t length;
	/** Payload according to the config type */
	uint8_t *payload;
	/** Reserved for future use */
	uint8_t reserved;
} __packed;

/**
 * @brief Configuration types.
*/
enum cs_router_config_type {
	CS_CONFIG_TYPE_WIFI_SSID,
	CS_CONFIG_TYPE_WIFI_PSK,
	CS_CONFIG_TYPE_UART_BAUDRATE,
	CS_CONFIG_TYPE_UART_BUFFER_SIZE
};

/**
 * @brief Persistence modes, how configuration should be stored.
 */
enum cs_router_config_persistence_mode {
	CS_CONFIG_PERSISTENCE_MODE_TEMPORARY,
	CS_CONFIG_PERSISTENCE_MODE_STORED
};

/**
 * @brief Frame struct for an operation packet
*/