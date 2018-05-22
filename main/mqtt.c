#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "cJSON.h"

#include "keybus_handler.h"

static const char *TAG = "mqtt";
static char * mqtt_queue_prefix;
static char * mqtt_write_queue;
static char * mqtt_msg_queue;
static char * my_id;

static esp_mqtt_client_handle_t mqtt_client = NULL;

char* create_keybus_msg_json(keybus_msg_t msg) { // Returned string must be free'd by caller / user
  char *json_string = NULL;
  bool create_periph_msg;
  cJSON *keybus_msg = cJSON_CreateObject();
  cJSON_AddStringToObject(keybus_msg, "source", my_id);
  cJSON *panel_msg = cJSON_CreateArray();
  cJSON *periph_msg;
  cJSON_AddItemToObject(keybus_msg, "panel_msg", panel_msg);
  create_periph_msg = periph_msg_present(msg.pmsg, msg.len_bytes);
  if (create_periph_msg) {
    periph_msg = cJSON_CreateArray();
    cJSON_AddItemToObject(keybus_msg, "perph_msg", periph_msg);
  }
  for (uint8_t i = 0; i < msg.len_bytes; i++) {
    cJSON_AddItemToArray(panel_msg, cJSON_CreateNumber(msg.msg[i]));
    if (create_periph_msg) {
      cJSON_AddItemToArray(periph_msg, cJSON_CreateNumber(msg.pmsg[i]));
    }
  }
  if (create_periph_msg && keybus_key_crc_ok(msg.pmsg[2])) {
    cJSON_AddItemToObject(keybus_msg, "key_press", cJSON_CreateNumber(msg.pmsg[2] >> 2));
  }
  json_string = cJSON_Print(keybus_msg);
  cJSON_Delete(keybus_msg);
  return json_string;
}

esp_err_t mqtt_dispatch_msg(keybus_msg_t msg) {
  if (mqtt_client) {
    char * json_string = create_keybus_msg_json(msg);
    ESP_LOGD(TAG, "Write msg: %s", json_string);
    esp_mqtt_client_publish(mqtt_client, mqtt_msg_queue, json_string, strlen(json_string), 0, 0);
    free(json_string);
  }
  return ESP_OK;
}

void handle_queue_message(char *topic, int topic_len, char *data, int data_len) {
  if (strncmp(mqtt_write_queue, topic, topic_len) == 0) {
    ESP_LOGI(TAG, "Received message on %s", mqtt_write_queue);
    cJSON *write_json = NULL;
    cJSON *write_element = NULL;
    char *json_string = malloc(data_len+1);
    memcpy(json_string, data, data_len);
    json_string[data_len] = '\0';
    cJSON *json = cJSON_Parse(json_string);
    write_json = cJSON_GetObjectItemCaseSensitive(json, "write");
    if (cJSON_IsString(write_json) && (write_json->valuestring != NULL)) {
      ESP_LOGW(TAG, "Received write string: %s, but writing strings not supported yet",
        write_json->valuestring);
        int i = 0;
        int k;
        while(write_json->valuestring[i] != '\0') {
          if (isspace((unsigned char)write_json->valuestring[i])) {
            i++;
            continue;
          }
          if (write_json->valuestring[i] >= 48 && write_json->valuestring[i] <= 57) {
    				k = keybus_add_crc(write_json->valuestring[i]-48);
          } else {
            k = keybus_add_crc(write_json->valuestring[i]);
          }
          ESP_LOGI(TAG, "Write %c [%x] to KeyBus\n", write_json->valuestring[i], k);
          xQueueSend(write_queue, &k, NULL);
          i++;
        }
    } else if (cJSON_IsArray(write_json)) {
      cJSON_ArrayForEach(write_element, write_json) {
        if (cJSON_IsNumber(write_element)) {
          // Write element
          int c = write_element->valueint;
          switch (c) {
            case '*':
              c = 0x0a;
              break;
            case '#':
              c = 0x0b;
              break;
          }
          int k = (c << 2) | ((((c >> 4) & 0x03) + ((c >> 2) & 0x03) + (c & 0x03)) & 0x03);
          ESP_LOGI(TAG, "Write %d [%x] to KeyBus\n", c, k);
          xQueueSend(write_queue, &k, NULL);
        }
      }
    }
    if (json == NULL) {
      const char *error_ptr = cJSON_GetErrorPtr();
       if (error_ptr != NULL)
       {
           ESP_LOGE(TAG, "Error before: %s\n", error_ptr);
       }
       goto end;
    }
    // Process JSON
end:
    free(json_string);
    cJSON_Delete(json);
  }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;

    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            msg_id = esp_mqtt_client_subscribe(client, mqtt_write_queue, 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d topic: %s", msg_id, mqtt_write_queue);
            mqtt_client = client;
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT_EVENT_DISCONNECTED");
            mqtt_client = NULL;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
            handle_queue_message(event->topic, event->topic_len, event->data, event->data_len);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
    }
    return ESP_OK;
}

esp_mqtt_client_handle_t mqtt_start(char * mqtt_uri)
{

    uint8_t mac[6];
    esp_err_t e = esp_base_mac_addr_get(mac);
    if (e != ESP_OK) {
      ESP_LOGE(TAG, "Couldnt read base MAC: %s", esp_err_to_name(e));
      e = esp_efuse_mac_get_default(mac);
      if (e != ESP_OK) {
        ESP_LOGE(TAG, "Couldnt default MAC: %s", esp_err_to_name(e));
        return NULL;
      }
    }
    //struct keybus_queues kb;
    mqtt_queue_prefix = malloc(32);
    mqtt_write_queue  = malloc(32);
    mqtt_msg_queue    = malloc(32);
    my_id             = malloc(18);
    sprintf(mqtt_queue_prefix, "/bitfire/esp32-dsc/%02x%02x%02x",       mac[3], mac[4], mac[5]);
    sprintf(mqtt_write_queue,  "/bitfire/esp32-dsc/%02x%02x%02x/write", mac[3], mac[4], mac[5]);
    sprintf(mqtt_msg_queue,    "/bitfire/esp32-dsc/%02x%02x%02x/msg",   mac[3], mac[4], mac[5]);
    sprintf(my_id,    "esp32-dsc-%02x%02x%02x",   mac[3], mac[4], mac[5]);
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_uri,
        .event_handle = mqtt_event_handler,
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    if (!client) {
      ESP_LOGE(TAG, "Failed to init MQTT client");
      return NULL;
    }
    esp_mqtt_client_start(client);
    return client;
}
