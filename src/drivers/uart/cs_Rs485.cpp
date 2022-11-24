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

static struct k_event evt_buf_released;
static struct k_mutex mut_buf_access, mut_inst_access;

/**
 * @brief Initialize the RS485 module.
 *
 * @return CS_OK if the RS485 module was sucessfully initialized.
 */
cs_err_t Rs485::init(struct cs_uart_config *cfg)
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
	uart_cfg.baudrate = CLAMP(cfg->baudrate, RS485_BAUD_MIN, RS485_BAUD_MAX);
	uart_cfg.flow_ctrl = UART_CFG_FLOW_CTRL_NONE;
	uart_cfg.data_bits = UART_CFG_DATA_BITS_8;

	switch (cfg->parity) {
	case UART_CFG_PARITY_ODD:
	case UART_CFG_PARITY_EVEN:
		uart_cfg.parity = cfg->parity;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_1;
		break;
	case UART_CFG_PARITY_NONE:
		// requires 2 stop bits
		uart_cfg.parity = cfg->parity;
		uart_cfg.stop_bits = UART_CFG_STOP_BITS_2;
		break;
	default:
		return CS_ERR_UART_CONFIG_INVALID;
	}

	if (uart_configure(rs485_dev, &uart_cfg) != 0) {
		LOG_ERR("Failed to configure uart");
		return CS_ERR_UART_CONFIG_FAILED;
	}

  k_event_init(&evt_buf_released);
  k_mutex_init(&mut_buf_access);

  int32_t uart_timeout = (NUM_BITS * MAX_DELAY) / uart_cfg.baudrate;
  uint8_t *rx_buf = (uint8_t*)k_calloc(UART_BUFFER_SIZE, sizeof(uint8_t));
	// start listening using the first buffer
	if (uart_rx_enable(rs485_dev, rx_buf, UART_BUFFER_SIZE, uart_timeout) != 0) {
		LOG_ERR("Failed to start listening on RX");
		return CS_ERR_UART_RX_FAILED;
	}

	return CS_OK;
}

/**
 * @brief Handle uart event.
 */
void Rs485::handle_uart_event(const struct device *dev, struct uart_event *evt, void *user_data)
{
	switch (evt->type) {
	case UART_RX_BUF_REQUEST:
		handle_rx_buf_rq(dev, evt);
		break;
	case UART_RX_RDY:
    handle_uart_data(dev, evt);
		break;
	case UART_RX_BUF_RELEASED:
		break;
	case UART_RX_DISABLED:
		break;
	case UART_RX_STOPPED:
		break;
	case UART_TX_DONE:
		break;
	case UART_TX_ABORTED:
		break;
	default:
		break;
	}
}

/**
 * @brief Handle uart buffer request event.
 * Provides a second buffer for continuous reception.
 */
void Rs485::handle_rx_buf_rq(const struct device *dev, struct uart_event *evt)
{
  uint8_t *new_rx_buf = (uint8_t*)k_calloc(UART_BUFFER_SIZE, sizeof(uint8_t));

  int ret = uart_rx_buf_rsp(dev, new_rx_buf, UART_BUFFER_SIZE);
  switch (ret) {
  case -EBUSY:
    LOG_ERR("Unable to set second buffer, buffer is already set");
    break;
  case -EACCES:
    LOG_ERR("Receiver is disabled, function might be called too late");
    break;
  default:
    LOG_ERR("Failed to set second buffer with unknown error");
    break;
  }
}

/**
 * @brief Handle uart data.
 * Happens after: rx timeout event, buffer full, uart disable and stopped
 * when there is data in the buffer.
*/
void Rs485::handle_uart_data(const struct device *dev, struct uart_event *evt)
{
	if (k_mutex_lock(&mut_buf_access, K_FOREVER) != 0) {
		return;
	}
  // allocate tmp buffer to handle the data without blocking access to uart buffer too long
  uint8_t *tmp_buf = (uint8_t*)k_malloc(evt->data.rx.len * sizeof(uint8_t));
  // only copy new received bytes: from buffer offset + len
  memcpy(evt->data.rx.buf + evt->data.rx.offset, tmp_buf, evt->data.rx.len);
  k_mutex_unlock(&mut_buf_access);

  if (k_mutex_lock(&mut_inst_access, K_FOREVER) != 0) {
		return;
	}
  Rs485 *rs485 = &Rs485::getInstance();
  
  for (int b = evt->data.rx.offset; b <= (evt->data.rx.offset + evt->data.rx.len); b++) {
    // store characters until line end is detected
    if ((tmp_buf[b] == '\n' || tmp_buf[b] == '\r') && rs485->_rx_msg_buf_ctr > 0) {
      // terminate string
      rs485->_rx_msg_buf[rs485->_rx_msg_buf_ctr] = '\0';
    }
  }
}