#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "config.h"
#include "esp_log.h"
#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"
#include "rom/rtc.h"
#include "soc/cpu.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"


#include "dsc_tcp.h"
#include "mqtt.h"

static const char* TAG = "config";
TaskHandle_t config_task_handle = NULL;
configuration_t dsc_config;

int load_config(bool initflash) {
  nvs_handle nvs_config;
  esp_err_t e;
  if (initflash)
    e = nvs_flash_init();
  else
    e = ESP_OK;

  if (e == ESP_OK) e = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READONLY, &nvs_config);
  if (e == ESP_OK) {
    size_t required_size = 0;
    e = nvs_get_str(nvs_config, "dscserver", NULL, &required_size);
    char* dscserver = malloc(required_size);
    e = nvs_get_str(nvs_config, "dscserver", dscserver, &required_size);
    if (e != ESP_OK) {
      dscserver = NULL;
      ESP_LOGE(TAG, "Couldn't read dscserver from flash: %s", esp_err_to_name(e));
      return -1;
    }
    uint8_t server_type = SERVER_TCP;
    e = nvs_get_u8(nvs_config, "server_type", &server_type);
    if (e != ESP_OK) ESP_LOGW(TAG, "Couldn't read server type, defaulting to SERVER_TCP: %s", esp_err_to_name(e));

    uint16_t port = 10242;
    e = nvs_get_u16(nvs_config, "server_port", &port);
    if (e != ESP_OK) ESP_LOGW(TAG, "Couldn't read server port, defaulting to 10242: %s", esp_err_to_name(e));
    dsc_config.port = port;
    dsc_config.dscserver = dscserver;
    dsc_config.server_type = (server_type_t)server_type;
    dsc_config.dispatch_msg = (server_type == SERVER_MQTT) ? mqtt_dispatch_msg : dsc_tcp_dispatch_msg;
    ESP_LOGI(TAG, "Config loaded:");
    ESP_LOGI(TAG, "Server: %s, Server Type: %d, Server Port: %d", dscserver, server_type, port);
    nvs_close(nvs_config);
    return ESP_OK;
  } else {
    dsc_config = (configuration_t) {
      .port = 10242,
      .dscserver = NULL,
      .server_type = SERVER_TCP,
      .dispatch_msg = dsc_tcp_dispatch_msg
    };
    ESP_LOGE(TAG, "Couldn't find config namespace, using hardcoded defaults: %s", esp_err_to_name(e));
    return -1;
  }
}
int erase_config() {
  nvs_handle nvs_config;
  esp_err_t e;
  ESP_LOGW(TAG, "Erasing all settings!")  ;
  if ((e = nvs_open(NVS_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_config)) == ESP_OK) {
    e = nvs_erase_all(nvs_config);
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "Error erasing NVS: %s", esp_err_to_name(e));
    }
    nvs_close(nvs_config);
  } else {
    ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(e));
    return -1;
  }
  return 0;
}

void erase_ota() {
  const esp_partition_t * f = NULL;
  f = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
  if (f != NULL) {
    esp_partition_erase_range(f, 0, f->size);
    ESP_LOGW(TAG, "Erased OTA Partition");
  } else {
    ESP_LOGE(TAG, "Couldnt find OTA partition!");
  }
}

void config_task(void *pvParameter) {
  uint8_t reset_button = 0;
  uint32_t ulNotifiedValue;
  ESP_LOGI(TAG, "Config task started");
  const esp_partition_t * f = NULL;
  gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,
    .mode = GPIO_MODE_INPUT,
    .pin_bit_mask = (1 << USER_GPIO),
    .pull_down_en = 0,
    .pull_up_en = 0,
  };
  gpio_config(&io_conf);
  while (true) {
    xTaskNotifyWait(0x00,ULONG_MAX,&ulNotifiedValue, (1000 / portTICK_RATE_MS));
    if ((ulNotifiedValue & RESET_FACTORY) != 0) {
      ESP_LOGW(TAG, "Factory reset");
      //const esp_partition_t * f = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
      f = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
      if (f != NULL) {
        esp_partition_erase_range(f, 0, f->size);
        ESP_LOGW(TAG, "Erased OTA Partition");
        esp_restart();
        return;
      } else {
        ESP_LOGE(TAG, "Couldnt find OTA partition!");
      }
    }
    if (gpio_get_level(USER_GPIO) ^ USER_GPIO_INVERTED) {
      reset_button++;
    } else {
      if (reset_button > 3 && reset_button < 10) {
        ESP_LOGW(TAG, "Boot to factory mode");
        erase_ota();
        esp_restart();
      } else if (reset_button > 10) {
        ESP_LOGW(TAG, "Factory reset triggered!");
        erase_config();
      }
      reset_button = 0;
    }
  }
}
