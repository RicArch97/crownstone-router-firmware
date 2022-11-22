/**
 * Author: Ricardo Steijn
 * Copyright: Crownstone (https://crownstone.rocks)
 * Date: 22 Nov., 2022
 * License: Apache License 2.0
 */

#pragma once

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include "cs_ReturnTypes.h"

class Rs485
{
public:
  static Rs485 &getInstance()
  {
    static Rs485 instance;
    return instance;
  }
  RS485(Rs485 const &) = delete;
  void operator=(Rs485 const &) = delete;

  cs_err_t init();

private:
  Rs485() {}
  ~Rs485();
  static void handle_uart_event(const struct device *dev, struct uart_event *evt, void *user_data)

  bool _isInitialized = false;
};