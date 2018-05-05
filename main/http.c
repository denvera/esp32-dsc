#include "config.h"

#include <string.h> /* memset */
#include <stdlib.h> /* atoi */
#include <stdio.h>
#include "esp_log.h"
#include "rom/cache.h"
#include "rom/ets_sys.h"
#include "rom/spi_flash.h"
#include "rom/crc.h"
#include "rom/rtc.h"
#include "esp_partition.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "httpd/include/httpd_opts.h"
#include "httpd/include/httpd.h"
#include "http.h"
#include "config.h"

static const char* TAG = "http";
#define NUM_SSI_TAGS 1
#define NUM_CGI_HANDLERS 1

static char const *actions_cgi_handler(int index, int numParams, char const *param[], char const *value[]);

static char const *  ssi_tags[] = {
    "s_ver"
};

static tCGI const cgi_handlers[] = {
    { "/actions.cgi", &actions_cgi_handler },
};

//return (char *)0;
static char const * actions_cgi_handler(int index, int numParams, char const *param[], char const *value[]) {
  //ESP_LOGI(TAG, "Action");
   for (int i = 0; i < numParams; ++i) {
         if (strstr(param[i], "action") != (char *)0) {      // param text found?
  //         ESP_LOGI(TAG, "Action");
           if (!strcmp(value[i], "factory")) {
             //nvs_flash_erase();
             //nvs_flash_erase_partition("otadata");
             // esp_partition_t *f = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
             // ESP_ERROR_CHECK(esp_ota_set_boot_partition(f))
             // esp_restart();
             //software_reset();
             xTaskNotify(config_task_handle, RESET_FACTORY, eSetValueWithOverwrite);
             return "/done.html";
           }
         }
     }
  return (char *)0;
}

static u16_t ssi_handler(int iIndex, char *pcInsert, int iInsertLen) {
  switch (iIndex) {
    case 0: // s_version
      return snprintf(pcInsert, LWIP_HTTPD_MAX_TAG_INSERT_LEN, VERSION);
  }
  return 0;
}

void httpd_cgi_handler(const char* uri, int iNumParams, char **pcParam, char **pcValue) {
  ESP_LOGI(TAG, "CGI: %s Params: %d", uri, iNumParams);
}

err_t httpd_post_begin(void *connection, const char *uri, const char *http_request,
                       u16_t http_request_len, int content_len, char *response_uri,
                       u16_t response_uri_len, u8_t *post_auto_wnd) {
// Handle post
  ESP_LOGI(TAG, "Post: URI: %s [%d]\n", uri, content_len);
  return ERR_OK;
}

err_t httpd_post_receive_data(void *connection, struct pbuf *p) {
  char *str = malloc(p->len + 1);
  memset(str, 0, p->len+1);
  strncpy(str, p->payload, p->len);
  printf("Post data: %s\n", str);
  free(p);
  free(str);
  return ERR_OK;
}

void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len) {
}

void setup_httpd(TaskHandle_t c) {
  config_task_handle = c;
  http_set_ssi_handler((tSSIHandler)ssi_handler, ssi_tags, NUM_SSI_TAGS);
  http_set_cgi_handlers((const tCGI *)cgi_handlers, NUM_CGI_HANDLERS);
  httpd_init();
}
