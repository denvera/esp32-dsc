#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "dsc_tcp.h"
#include "config.h"
#include "simple_wifi.h"
#include "keybus_handler.h"

static const char *TAG = "dsc_tcp";
static int dsc_socket = NULL;
static bool connected = false;
bool binary_mode = false;

int dsc_tcp_write(int s, char * msg) {
  int r = write(s, msg, strlen(msg));
  if (r < 0) {
    ESP_LOGE(TAG, "... socket send failed");
    connected = false;
    close(dsc_socket);
  } else {
    ESP_LOGI(TAG, "... socket send success");
  }
  return r;
}

esp_err_t dsc_tcp_dispatch_msg(keybus_msg_t msg) {
  if (dsc_socket != NULL) {
    //if (!binary_mode) {
      char msg_buf[128] = "P: ";
      char p_msg_buf[128] = "C: ";
      format_msg(msg.msg, msg.len_bytes, msg_buf+3);
      format_msg(msg.pmsg, msg.len_bytes, p_msg_buf+3);
      strcat(msg_buf, "\r\n");
      strcat(p_msg_buf, "\r\n");
      ESP_LOGD(TAG, "Dispatch message");
    //} else {

    //}
    dsc_tcp_write(dsc_socket, msg_buf);
    if (periph_msg_present(msg.pmsg, msg.len_bytes))
      dsc_tcp_write(dsc_socket, p_msg_buf);
  } else {
    ESP_LOGW(TAG, "Attempt to dispatch to NULL socket");
  }
  return ESP_OK;
}

void dsc_tcp_task(void *pvParameters) {
  char * dsc_server = dsc_config.dscserver;
  int port = dsc_config.port;
  char s_port[16];
  char recv_buf[256];
  const struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo *res;
  struct in_addr *addr;
  int r = NULL;
  keybus_msg_t msg;

  sprintf(s_port, "%d", port);

  while (true) {
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                           false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP, trying to connect to DSC TCP server");
    int err = getaddrinfo(dsc_server, s_port, &hints, &res);
    addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
    ESP_LOGI(TAG, "DNS lookup succeeded. IP=%s", inet_ntoa(*addr));
    dsc_socket = socket(res->ai_family, res->ai_socktype, 0);
    if(dsc_socket < 0) {
        ESP_LOGE(TAG, "... Failed to allocate socket.");
        freeaddrinfo(res);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        connected = false;
        continue;
    }
    ESP_LOGI(TAG, "... allocated socket");

    if(connect(dsc_socket, res->ai_addr, res->ai_addrlen) != 0) {
        ESP_LOGE(TAG, "... socket connect failed errno=%d", errno);
        connected = false;
        close(dsc_socket);
        freeaddrinfo(res);
        vTaskDelay(4000 / portTICK_PERIOD_MS);
        continue;
    }
    ESP_LOGI(TAG, "... connected");
    freeaddrinfo(res);
    struct timeval receiving_timeout;
    receiving_timeout.tv_sec = 5;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(dsc_socket, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
      ESP_LOGE(TAG, "... failed to set socket receiving timeout");
      close(dsc_socket);
      vTaskDelay(4000 / portTICK_PERIOD_MS);
      connected = false;
      continue;
    }

    connected = true;
    while (connected) {
      bzero(recv_buf, sizeof(recv_buf));
      r = read(dsc_socket, recv_buf, sizeof(recv_buf)-1);
      if (r > 0) {
        ESP_LOGD(TAG, "Read: %s", recv_buf);
      } else {
        ESP_LOGD(TAG, "Read timeout");
      }
      // if (response_queue == NULL) {
      //   ESP_LOGW(TAG, "Response queue NULL...retry in 1s");
      //   vTaskDelay(1000 / portTICK_PERIOD_MS);
      //   continue;
      // }
      //xQueueReceive(response_queue, &msg, portMAX_DELAY); // Switched to dispatch functions

    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
