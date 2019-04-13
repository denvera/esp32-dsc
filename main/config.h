#ifndef CONFIG_H
#define CONFIG_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "keybus_handler.h"
#define VERSION "1.0.5"

#define USER_GPIO GPIO_NUM_0
#define USER_GPIO_INVERTED 1
#define LED_GPIO GPIO_NUM_2

#define LWIP_HTTPD_CGI_SSI 1
#define LWIP_HTTPD_SSI 1
#define LWIP_HTTPD_CGI 1
#define LWIP_HTTPD_SUPPORT_POST 1

#define RESET_FACTORY 1
#define NVS_CONFIG_NAMESPACE "config"

typedef enum {SERVER_MQTT, SERVER_TCP, SERVER_CLOUD} server_type_t;

typedef struct {
  char * dscserver;
  int port;
  server_type_t server_type;
  esp_err_t (*dispatch_msg)(keybus_msg_t);
} configuration_t;

extern configuration_t dsc_config;

extern TaskHandle_t config_task_handle;

int load_config(bool initflash);
void config_task(void *pvParameter);
void erase_ota();

#endif
