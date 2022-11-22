/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

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
cs_err_t Rs485::init()
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
  uart_callback_set(rs485_dev, handle_uart_event, NULL);
}

/**
 * @brief Handle uart event.
*/
void Rs485::handle_uart_event(const struct device *dev, struct uart_event *evt, void *user_data)
{

}