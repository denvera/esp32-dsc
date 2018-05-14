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
#include "esp_timer.h"
#include "esp_types.h"
#include "nvs_flash.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "libesphttpd/route.h"
#include "libesphttpd/webpages-espfs.h"
#include "libesphttpd/espfs.h"
#include "libesphttpd/httpdespfs.h"
#include "libesphttpd/httpd-freertos.h"

#include "http.h"
#include "config.h"
#include "wifi.h"

static const char* TAG = "http";

HttpdInstance httpdInstance;
HttpdFreertosInstance httpdFreeRtosInstance;

HttpdBuiltInUrl builtInUrls[]={
  ROUTE_REDIRECT("/", "/index.tpl"),
  ROUTE_TPL("/index.tpl", tplIndex),
  ROUTE_REDIRECT("/settings", "/settings/settings.tpl"),
  ROUTE_REDIRECT("/settings/", "/settings/settings.tpl"),
  ROUTE_TPL("/settings/settings.tpl", tplSettings),
  ROUTE_CGI("/settings/reboot.cgi", cgiReboot),
  ROUTE_CGI("/settings/settings.cgi", cgiSettings),
  //ROUTE_REDIRECT("/dsc", "/dsc/index.html"),
	//ROUTE_REDIRECT("/dsc/", "/dsc/index.html"),
	//ROUTE_CGI("/dsc/dscconf.cgi", cgiDSCConf),

  ROUTE_FILESYSTEM(),
  ROUTE_END()
};

CgiStatus ICACHE_FLASH_ATTR tplIndex(HttpdConnData *connData, char *token, void **arg) {
  if (token==NULL) return HTTPD_CGI_DONE;
  if (strcmp(token, "uptime") == 0) {
    char buf[128];
    int len;
    int64_t uptime_us = esp_timer_get_time();
    int days = (uptime_us / 1000 / 1000 / 3600 / 24);
    int hrs = (uptime_us / 1000 / 1000 / 3600) % 24;
    int min = (uptime_us / 1000 / 1000 /60) % 60;
    int s = (uptime_us / 1000 / 1000) % 60;
    len = snprintf(buf, sizeof(buf), "%d day%s %d hr%s %d min %d s",
          days, ((days != 1) ? "s" : ""), hrs, ((hrs != 1) ? "s" : ""),  min, s);
    httpdSend(connData, buf, len);
  } else if (strcmp(token, "version") == 0) {
    httpdSend(connData, VERSION, -1);
  } else if (strcmp(token , "heap") == 0) {
    char buf[32];
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    int len = snprintf(buf, sizeof(buf), "%d bytes", free_heap);
    httpdSend(connData, buf, len);
  }
  return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR tplSettings(HttpdConnData *connData, char *token, void **arg) {
  if (token==NULL) return HTTPD_CGI_DONE;
  int len;
  if (strcmp(token, "dscserver") == 0) {
    char buf[128];
    if (dsc_config.dscserver != NULL) {
      len = snprintf(buf, sizeof(buf), "%s", dsc_config.dscserver);
      httpdSend(connData, buf, len);
    } else {
      httpdSend(connData, "Not Set", -1);
    }
  } else if (strcmp(token, "version") == 0) {
    httpdSend(connData, VERSION, -1);
  } else if (strcmp(token, "port") == 0) {
    char s_port[32];
    len = snprintf(s_port, sizeof(s_port), "%d", dsc_config.port);
    httpdSend(connData, s_port, len);
  } else if (strcmp(token, "servertype") == 0) {
    const char *servertypes[] = {"MQTT", "TCP", "Cloud"};
    const char *s_servertype = servertypes[dsc_config.server_type];

    httpdSend(connData, s_servertype, strlen(s_servertype));
    }
  return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR cgiSettings(HttpdConnData *connData) {
  char dscserver[32];
  char s_servertype[32], s_port[32];
  uint8_t servertype;
  //uint16_t port;
  nvs_handle nvs;
  ESP_ERROR_CHECK(nvs_open("config", NVS_READWRITE, &nvs));
  if (httpdFindArg(connData->post.buff, "dscserver", dscserver, sizeof(dscserver)) > 0) {
    ESP_LOGW(TAG, "Setting DSC Server to %s", dscserver);
    ESP_ERROR_CHECK(nvs_set_str(nvs, "dscserver", dscserver));
  }
  if (httpdFindArg(connData->post.buff, "servertype", s_servertype, sizeof(s_servertype)) > 0) {
    servertype = (strcmp(s_servertype, "MQTT") == 0) ? SERVER_MQTT : SERVER_TCP;
    ESP_LOGW(TAG, "Setting server type to %s", s_servertype);
    ESP_ERROR_CHECK(nvs_set_u8(nvs, "server_type", servertype));
  } else {
    ESP_ERROR_CHECK(nvs_set_u8(nvs, "servertype", SERVER_TCP));
  }
  if (httpdFindArg(connData->post.buff, "port", s_port, sizeof(s_port)) > 0) {
    ESP_LOGW(TAG, "Setting port to %d", atoi(s_port));
ESP_ERROR_CHECK(nvs_set_u16(nvs, "server_port", atoi(s_port)));
  } else {
    ESP_ERROR_CHECK(nvs_set_u16(nvs, "server_port", 10242));
  }
  ESP_ERROR_CHECK(nvs_commit(nvs));
  nvs_close(nvs);
  //load_config(false);
  httpdRedirect(connData, "/done.html");
  return HTTPD_CGI_DONE;
}

CgiStatus ICACHE_FLASH_ATTR cgiReboot(HttpdConnData *connData) {
  char buf[128];
  int len;

  if (connData->isConnectionClosed) {
    //Connection aborted. Clean up.
    return HTTPD_CGI_DONE;
  }
  if (connData->requestType!=HTTPD_METHOD_POST) {
    //Sorry, we only accept GET requests.
    httpdStartResponse(connData, 406);  //http error code 'unacceptable'
    httpdEndHeaders(connData);
    return HTTPD_CGI_DONE;
  }

  len=httpdFindArg(connData->post.buff, "rebootfactory", buf, sizeof(buf));
  if (len != 0) {
    //if (strcmp(buf, "factory") == 0) {
      ESP_LOGW(TAG, "Factory Reboot");
      xTaskNotify(config_task_handle, RESET_FACTORY, eSetValueWithOverwrite);
      httpdRedirect(connData, "/done.html");
    //}
  } else {

    httpdSend(connData, "Unknown reboot type", -1);
  }
  return HTTPD_CGI_DONE;
}


void setup_httpd(TaskHandle_t c) {
  config_task_handle = c;
  xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                         false, true, portMAX_DELAY);
  espFsInit((void*)(webpages_espfs_start));
  void * connBuf = malloc(sizeof(RtosConnType) * 16);
  if (!connBuf) {
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGE(TAG, "Requested %d bytes, %d free", (sizeof(RtosConnType) * 16), free_heap);
    ESP_LOGE(TAG, "malloc failed: %d", errno);
    return;
  }
	httpdFreertosInit(&httpdFreeRtosInstance, builtInUrls, 80, connBuf, 16, HTTPD_FLAG_NONE);
  httpdInstance = httpdFreeRtosInstance.httpdInstance;

}
