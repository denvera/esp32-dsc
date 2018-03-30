#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"

#include "dsc_console.h"
#include "keybus_interface.h"
#include "keybus_handler.h"

#define VERSION "1.0.1"
#define AUTHORS "Denver Abrey [denver@bitfire.co.za]"


void app_main()
{
    printf("ESP32 DSC Gateway v%s\n", VERSION);
    printf("%s\n", AUTHORS);

    /* Print chip information */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    printf("This is ESP32 DSC Gateway with %d CPU cores, WiFi%s%s, ",
            chip_info.cores,
            (chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
            (chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");

    printf("silicon revision %d, ", chip_info.revision);

    printf("%dMB %s flash\n", spi_flash_get_chip_size() / (1024 * 1024),
            (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded" : "external");

    initialize_console();
    xTaskCreatePinnedToCore(&console_task, "console_task_pro_cpu", 8192, NULL, 0, NULL, 0); // App CPU, Priority 10
    keybus_init();
    printf("Starting KeyBus Task...");
    xTaskCreatePinnedToCore(&keybus_handler_task, "keybus_task_app_cpu", 8192, NULL, (configMAX_PRIORITIES-1), NULL, 1); // App CPU, Priority 10
    printf("Done\n");
    fflush(stdout);
}
