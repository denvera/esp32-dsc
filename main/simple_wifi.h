#include "freertos/event_groups.h"

/* FreeRTOS event group to signal when we are connected*/
extern EventGroupHandle_t wifi_event_group;
extern const int WIFI_CONNECTED_BIT;
void simple_wifi_init();
