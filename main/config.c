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
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"


static const char* TAG = "config";

void config_task(void *pvParameter) {
  uint8_t reset_button = 0;
  uint32_t ulNotifiedValue;
  ESP_LOGI(TAG, "Config task started");
  const esp_partition_t * f = NULL;
  while (true) {
    xTaskNotifyWait(0x00,ULONG_MAX,&ulNotifiedValue, pdMS_TO_TICKS(1000));
    if ((ulNotifiedValue & RESET_FACTORY) != 0) {
      ESP_LOGW(TAG, "Factory reset");
      //const esp_partition_t * f = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, NULL);
      f = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_OTA, NULL);
      if (f != NULL) {
        esp_partition_erase_range(f, 0, f->size);
        esp_restart();
      } else {
        ESP_LOGE(TAG, "Couldnt find OTA partition!");
      }
    }
  }
}
