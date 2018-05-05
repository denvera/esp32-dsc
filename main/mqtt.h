#ifndef MQTT_H
#define MQTT_H

#include "mqtt_client.h"
#include "keybus_handler.h"

esp_mqtt_client_handle_t mqtt_start(char * mqtt_uri);
esp_err_t mqtt_dispatch_msg(keybus_msg_t msg);

#endif
