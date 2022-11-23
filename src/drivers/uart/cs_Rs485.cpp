/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#include <errno.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(cs_Rs485, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include "cs_ReturnTypes.h"
#include "drivers/uart/cs_Rs485.h"

#define RS485_UART_MODULE DT_NODELABEL(uart0)

/**
 * @brief Initialize the RS485 module.
 *
 * @return CS_OK if the RS485 module was sucessfully initialized.
 */
cs_err_t Rs485::init(uint8_t baudrate)
{
  if (_isInitialized) {
    LOG_ERR("Already initialized");
    return CS_ERR_ALREADY_INITIALIZED;
  }

  const struct device *rs485_dev = DEVICE_DT_GET(RS485_UART_MODULE);
  // obtain a device reference and check if device is ready
	if (!device_is_ready(rs485_dev)) {
		LOG_ERR("RS485 device %s is not ready", rs485_dev->name);
		return CS_ERR_DEVICE_NOT_READY;
	}

  // configure callback to receive data asynchronously
  int ret = uart_callback_set(rs485_dev, handle_uart_event, NULL);
  switch (ret) {
  case -ENOSYS:
    LOG_ERR("Asynchronous api not supported by this device");
    return CS_ERR_NOT_SUPPORTED;
  case -ENOTSUP:
    LOG_ERR("Asynchronous api not enabled");
    return CS_ERR_NOT_ENABLED;
  default:
    break;
  }

  // configure uart parameters
  struct uart_config uart_cfg = {0};
  uart_cfg.baudrate = baudrate;
  uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
  // use most common serial implematentation: 8-N-1
  uart_cfg.data_bits = UART_CFG_DATA_BITS_8;
  uart_cfg.parity = UART_CFG_PARITY_NONE;
  uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;

  if (uart_configure(rs485_dev, &uart_cfg) != 0) {
		LOG_ERR("Failed to configure uart");
		return CS_ERR_UART_CONFIG_FAILED;
	}

  return CS_OK;
}

/**
 * @brief Handle uart event.
*/
void Rs485::handle_uart_event(const struct device *dev, struct uart_event *evt, void *user_data)
{

}