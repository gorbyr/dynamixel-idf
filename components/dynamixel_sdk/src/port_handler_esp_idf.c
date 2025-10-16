#include "port_handler_esp_idf.h"

#include <rom/ets_sys.h>
#include <esp_timer.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "hal/uart_hal.h"

#define LATENCY_TIMER 16

#define TAG "DYNAMIXEL_SDK"

typedef struct {
  bool inuse;
  bool opened;
  char port_name[10];

  // ESP-IDF
  int uart_num;
  int tx_pin;
  int rx_pin;
  int en_pin;
  int baudrate;

  // esp_timer_get_time() is the best esp-idf timer, in microseconds
  int64_t packet_start_time;  // MICROSECS
  int64_t packet_timeout;     // MICROSECS
  int64_t tx_time_per_byte;   // NANOSECS
} PortData;

static PortData *portData = 0;

int portHandlerEspIdf(const char *port_name) {
  int port_num;

  if (0 == strcmp("UART0", port_name)) {
    port_num = 0;
  } else if (0 == strcmp("UART1", port_name)) {
    port_num = 1;
  } else if (0 == strcmp("UART2", port_name)) {
    port_num = 2;
  } else {
    ESP_LOGI(TAG, "PortHandler: port name %s is invalid", port_name);
    return (-1);
  }

  // first time
  if (portData == NULL) {
    // Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    ESP_LOGI(TAG, "Start Dynamixel Library for ESP-IDF");

    portData = malloc(PORT_HANDLER_ESP_IDF_UART_MAX * sizeof(PortData));
    if (!portData) {
      ESP_LOGI(TAG, "PortHandler: malloc failure");
      return (-1);
    }

    for (int i = 0; i < PORT_HANDLER_ESP_IDF_UART_MAX; i++) {
      portData[i].inuse = false;
    }

    g_used_port_num = PORT_HANDLER_ESP_IDF_UART_MAX;
  }

  PortData *pd = &portData[port_num];

  if (pd->inuse) {
    ESP_LOGI(TAG, "attempted to open port %s twice, failing", port_name);
    return (-1);
  }

  switch (port_num) {
    case 0:
      pd->tx_pin = PORT_HANDLER_ESP_IDF_UART0_TXPIN;
      pd->rx_pin = PORT_HANDLER_ESP_IDF_UART0_RXPIN;
      pd->en_pin = PORT_HANDLER_ESP_IDF_UART0_ENPIN;
      break;

    case 1:
      pd->tx_pin = PORT_HANDLER_ESP_IDF_UART1_TXPIN;
      pd->rx_pin = PORT_HANDLER_ESP_IDF_UART1_RXPIN;
      pd->en_pin = PORT_HANDLER_ESP_IDF_UART1_ENPIN;
      break;

    case 2:
      pd->tx_pin = PORT_HANDLER_ESP_IDF_UART2_TXPIN;
      pd->rx_pin = PORT_HANDLER_ESP_IDF_UART2_RXPIN;
      pd->en_pin = PORT_HANDLER_ESP_IDF_UART2_ENPIN;
      break;

    default:
      ESP_LOGI(TAG, "attempted to open port %s twice, failing", port_name);
      return (-1);
  }

  pd->uart_num = port_num;
  strncpy(&pd->port_name[0], port_name, sizeof(pd->port_name) - 1);
  pd->opened = false;
  pd->inuse = true;

  pd->baudrate = DEFAULT_BAUDRATE;
  pd->packet_start_time = 0;
  pd->packet_timeout = 0;
  pd->tx_time_per_byte = 0;

  return port_num;
}

uint8_t setupPortEspIdf(int port_num) {
  PortData *pd = &portData[port_num];

  uart_config_t uart_config = {
      .baud_rate = pd->baudrate,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 0,  // but disabled?
      .source_clk = UART_SCLK_APB,
  };

  if (pd->opened) {
    ESP_LOGI(TAG, "setupPort called when already setup, don't do that");
    return False;
  }

  int uart_num = pd->uart_num;

  if (uart_is_driver_installed(uart_num)) {
    ESP_LOGI(TAG, "setupPort unexpected uart driver already installed");
    uart_driver_delete(uart_num);
  }

  // Install UART driver (we don't need an event queue here)
  // In this example we don't even use a buffer for sending data.
  // NOTE this requires config UART setting - should acutally be based on what
  // the config settings is - TODO ESP_INTR_FLAG_IRAM

  ESP_LOGI(TAG, "setupPort setting all paramters uart %d baud %d", uart_num, uart_config.baud_rate);

  // Configure UART parameters
  ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

  // Set UART pins --- before setting up the driver, since the default pins
  // conflict with all the things
  // ESP_ERROR_CHECK(uart_set_pin(uart_num, pd->tx_pin, pd->rx_pin,
  // UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
  ESP_ERROR_CHECK(uart_set_pin(uart_num, pd->tx_pin, pd->rx_pin, 0, 0));

  ESP_LOGI(TAG, "setupPort calling uart driver install uart %d", uart_num);

  // Set read timeout of UART TOUT feature - this is in symbols - 0 disables
  // feature
  ESP_ERROR_CHECK(uart_set_rx_timeout(uart_num, 0 /* timeout */));

  // ESP_ERROR_CHECK(uart_driver_install(uart_num, 1024, 1024,
  //   0 /*qsize*/, NULL /*q handler*/, 0 /*intr alloc flags*/));
  //  NB - better to have no transmit buffer, so we don't have to worry about
  //  waiting for it to drain?
  // int r = uart_driver_install(uart_num, 512 /*rx buffer*/, 512 /* tx buffer
  // */ , 0 /*qsize*/, NULL /*q handler*/, 0 /*intr alloc flags*/);
  int r = uart_driver_install(uart_num, 512 /*rx buffer*/, 0 /* tx buffer */,
                              0 /*qsize*/, NULL /*q handler*/,
                              0 /*intr alloc flags*/);
  ESP_LOGI(TAG, "Uart Driver Install: uart num %d", uart_num);
  if (r != ESP_OK) return (False);

  // Must be set after driver is installed
  ESP_ERROR_CHECK(uart_set_mode(uart_num, UART_MODE_RS485_HALF_DUPLEX));

  // in nanos (10**9) for accuracy
  // and 10 bits per byte because of stop
  pd->tx_time_per_byte = (1000000000 / pd->baudrate) * 10;

  // set up the enable-dsable pin
  gpio_reset_pin(pd->en_pin);
  /* Set the GPIO as a push/pull output */
  gpio_set_direction(pd->en_pin, GPIO_MODE_OUTPUT);

  gpio_set_pull_mode(pd->en_pin, GPIO_PULLDOWN_ONLY);
  /* set low */
  gpio_set_level(pd->en_pin, 0);

  pd->opened = true;

  return True;  // success is generall 0. Boooooo.
}
// uint8_t setCustomBaudrateEspIdf(int port_num, int speed) {}
// int getCFlagBaud(const int baudrate) {}

double getCurrentTimeEspIdf() { return esp_timer_get_time(); }
double getTimeSinceStartEspIdf(int port_num) {
  return (
      (double)(esp_timer_get_time() - portData[port_num].packet_start_time) *
      1000.0);
}

uint8_t openPortEspIdf(int port_num) { return setupPortEspIdf(port_num); }
void closePortEspIdf(int port_num) {
  if (portData[port_num].inuse) {
    if (portData[port_num].opened) {
      ESP_ERROR_CHECK(uart_driver_delete(portData[port_num].uart_num));
      portData[port_num].opened = false;
    }

    ESP_LOGI(TAG, "Close Port EspIdf: delete driver: uart %d", portData[port_num].uart_num);
    ESP_ERROR_CHECK(uart_driver_delete(portData[port_num].uart_num));
    portData[port_num].inuse = false;
  }
}
void clearPortEspIdf(int port_num) { uart_flush(portData[port_num].uart_num); }

// void setPortNameEspIdf(int port_num, const char *port_name) {}
char *getPortNameEspIdf(int port_num) { return portData[port_num].port_name; }

uint8_t setBaudRateEspIdf(int port_num, const int baudrate) {
  if (portData[port_num].opened) {
    ESP_LOGI(TAG, " do not set baud rate after opening, close first ");
    return False;
  }

  portData[port_num].baudrate = baudrate;

  return True;
}
int getBaudRateEspIdf(int port_num) { return portData[port_num].baudrate; }

int getBytesAvailableEspIdf(int port_num) {
  size_t bytes_available;
  if (ESP_OK != uart_get_buffered_data_len(portData[port_num].uart_num, &bytes_available)) {
    ESP_LOGI(TAG, "Error on uarg_get_buffered_data_len");
    bytes_available = 0;
  }
  return bytes_available;
}

int IRAM_ATTR readPortEspIdf(int port_num, uint8_t *packet, int length) {
  int read_len = uart_read_bytes(portData[port_num].uart_num, packet, length, 150 /*ticks to wait*/);
  if (read_len < 0) {
    // not sure what right here
    ESP_LOGI(TAG, "readPortEspIdf: uart_read_bytes returned error, clamped to 0");
    read_len = 0;
  }

  return (read_len);
}
int IRAM_ATTR writePortEspIdf(int port_num, uint8_t *packet, int length) {
  /* enable while transmitting */
  gpio_set_level(portData[port_num].en_pin, 1);

  // note: this copies to the Tx FIFO buffer, it doesn't send it - that should
  // be OK other choices are uart_wait_tx_done(), which waits until the TX buff
  // is drained, or uart_tx_chars(), which is non-blocking and returns the
  // number of bytes written
  int r = uart_write_bytes(portData[port_num].uart_num, (const char *)packet, length);
  if (r < 0) {
    ESP_LOGI(TAG, "writePortEspIdf: uart_write_bytes returned %d unexpected", r);
    gpio_set_level(portData[port_num].en_pin, 0);
    return (0);
  }

  // Trying to speed things up by cutting out all the middlepeople and ignoring
  // the timouts, and using the smallest possible delay

  // low level code to check if the output buffer is flushed.
  // commenting it out because the GPIO is going too slow at 4Mb
  // this works great if the Servo has a 20us delay. However, it seems we're
  // holding the gpio line up about 150us longer than we need using this, which
  // doesn't work at 4mhz with 0 delay (just by a little).
  while (false == uart_ll_is_tx_idle(UART_LL_GET_HW(portData[port_num].uart_num))) {
    ets_delay_us(1);
  }

  // disable when done
  gpio_set_level(portData[port_num].en_pin, 0);

  return (r);
}

void setPacketTimeoutEspIdf(int port_num, uint16_t packet_length) {
  portData[port_num].packet_start_time = esp_timer_get_time();
  portData[port_num].packet_timeout =
      ((portData[port_num].tx_time_per_byte * packet_length) / 1000) +
      (LATENCY_TIMER * 2.0) + 2.0;
}
void setPacketTimeoutMSecEspIdf(int port_num, double msec) {
  portData[port_num].packet_start_time = esp_timer_get_time();
  portData[port_num].packet_timeout = msec;
}
uint8_t isPacketTimeoutEspIdf(int port_num) {
  double tss = getTimeSinceStartEspIdf(port_num);
  if (tss > portData[port_num].packet_timeout) {
    portData[port_num].packet_timeout = 0;
    return True;
  }
  return False;
}
