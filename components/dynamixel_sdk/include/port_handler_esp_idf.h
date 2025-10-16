#ifndef DYNAMIXEL_SDK_INCLUDE_DYNAMIXEL_SDK_ESPIDF_PORTHANDLERESPIDF_C_H_
#define DYNAMIXEL_SDK_INCLUDE_DYNAMIXEL_SDK_ESPIDF_PORTHANDLERESPIDF_C_H_

#include <stdint.h>

#include "port_handler.h"

// PIN configuration goes here (for the moment)
#define PORT_HANDLER_ESP_IDF_UART0_TXPIN 0
#define PORT_HANDLER_ESP_IDF_UART0_RXPIN 0
#define PORT_HANDLER_ESP_IDF_UART0_ENPIN 0

#define PORT_HANDLER_ESP_IDF_UART1_TXPIN (17)  // default: 10
#define PORT_HANDLER_ESP_IDF_UART1_RXPIN (18)  // default: 9
#define PORT_HANDLER_ESP_IDF_UART1_ENPIN (16)

#define PORT_HANDLER_ESP_IDF_UART2_TXPIN 0
#define PORT_HANDLER_ESP_IDF_UART2_RXPIN 0
#define PORT_HANDLER_ESP_IDF_UART2_ENPIN 0

#define PORT_HANDLER_ESP_IDF_UART_MAX 3

#define PORT_HANDLER_ESP_IDF_BUF_SIZE 1024

int portHandlerEspIdf(const char *port_name);

uint8_t setupPortEspIdf(int port_num);
uint8_t setCustomBaudrateEspIdf(int port_num, int speed);
int getCFlagBaud(const int baudrate);

double getCurrentTimeEspIdf();
double getTimeSinceStartEspIdf(int port_num);

uint8_t openPortEspIdf(int port_num);
void closePortEspIdf(int port_num);
void clearPortEspIdf(int port_num);

void setPortNameEspIdf(int port_num, const char *port_name);
char *getPortNameEspIdf(int port_num);

uint8_t setBaudRateEspIdf(int port_num, const int baudrate);
int getBaudRateEspIdf(int port_num);

int getBytesAvailableEspIdf(int port_num);

int readPortEspIdf(int port_num, uint8_t *packet, int length);
int writePortEspIdf(int port_num, uint8_t *packet, int length);

void setPacketTimeoutEspIdf(int port_num, uint16_t packet_length);
void setPacketTimeoutMSecEspIdf(int port_num, double msec);
uint8_t isPacketTimeoutEspIdf(int port_num);

#endif /* DYNAMIXEL_SDK_INCLUDE_DYNAMIXEL_SDK_ESPIDF_PORTHANDLERESPIDF_C_H_ */
