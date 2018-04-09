#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"

#include "config.h"
#include "dsc_console.h"

#ifdef CONFIG_KEYBUS_IMPL_TIMERS
#include "keybus_interface_timers.h"
#endif
#ifdef CONFIG_KEYBUS_IMPL_IRQ
#include "keybus_interface_irq.h"
#endif


#include "keybus_handler.h"

#include "httpd/include/httpd.h"

#include "simple_wifi.h"

#define AUTHORS "Denver Abrey [denver@bitfire.co.za]"

static const char* TAG = "esp32-dsc";


void app_main()
{
    ESP_LOGW(TAG, "ESP32 DSC Gateway v%s\n", VERSION);
    ESP_LOGW(TAG, "%s\n", AUTHORS);

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is ESP32 DSC Gateway with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    ESP_LOGI(TAG, "silicon revision %d, ", chip_info.revision);

    ESP_LOGI(TAG, "%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    initialize_console();
    ESP_LOGI(TAG, "Starting console task...");
    xTaskCreatePinnedToCore(&console_task, "console_task_pro_cpu", 8192, NULL, 0, NULL, 0); // App CPU, Priority 10
    keybus_init();
    ESP_LOGI(TAG, "Starting KeyBus handler task...");
    xTaskCreatePinnedToCore(&keybus_handler_task, "keybus_task_app_cpu", 8192, NULL, 0, NULL, 0); // App CPU, Priority 10
    ESP_LOGI(TAG, "Starting WiFi...");
    simple_wifi_init();
    ESP_LOGI(TAG, "Starting HTTPD...");
    httpd_init();
}
