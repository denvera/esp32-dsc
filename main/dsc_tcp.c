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
#include "wifi.h"
#include "keybus_handler.h"

static const char *TAG = "dsc_tcp";
static int dsc_socket = NULL;
static bool connected = false;
bool binary_mode = false;

int dsc_tcp_write(int s, char * msg, int len) {
  int r = write(s, msg, len);
  if (r < 0) {
    ESP_LOGE(TAG, "... socket send failed");
    connected = false;
    close(dsc_socket);
  } else {
    ESP_LOGD(TAG, "... socket send success");
  }
  return r;
}

esp_err_t dsc_tcp_dispatch_msg(keybus_msg_t msg) {
  if (dsc_socket != NULL) {
    int write_len;
    char msg_buf[128] = "P: ";
    char p_msg_buf[128] = "C: ";
    if (!binary_mode) {
      format_msg(msg.msg, msg.len_bytes, msg_buf+3);
      format_msg(msg.pmsg, msg.len_bytes, p_msg_buf+3);
      strcat(msg_buf, "\r\n");
      strcat(p_msg_buf, "\r\n");
      ESP_LOGD(TAG, "Dispatch message");
      dsc_tcp_write(dsc_socket, msg_buf, strlen(msg_buf));
      if (periph_msg_present(msg.pmsg, msg.len_bytes))
        dsc_tcp_write(dsc_socket, p_msg_buf, strlen(p_msg_buf));
    } else {
      msg_buf[0] = (msg.len_bytes+2) & 0xff;
      p_msg_buf[0] = (msg.len_bytes+2) & 0xff;
      switch(msg.msg[0]) {
        case 0x05:
          msg_buf[1] = DSC_STATUS;
          break;
        default:
          msg_buf[1] = DSC_PANEL;
      }
      memcpy(msg_buf+2, msg.msg, msg.len_bytes);
      dsc_tcp_write(dsc_socket, msg_buf, msg.len_bytes+2);
      if (periph_msg_present(msg.pmsg, msg.len_bytes)) {
        p_msg_buf[1] = DSC_CLIENT;
        memcpy(p_msg_buf+2, msg.pmsg, msg.len_bytes);
        dsc_tcp_write(dsc_socket, p_msg_buf, msg.len_bytes+2);
      }
    }
  } else {
    ESP_LOGW(TAG, "Attempt to dispatch to NULL socket");
  }
  return ESP_OK;
}
void dsc_tcp_handle_msg(char *line) {
  ESP_LOGI(TAG, "Handle Line: %s", line);
  int len = strlen(line);
	int i = 7;
	char c, k;
	if (strncmp(line, "WRITE: ", strlen("WRITE: ")) == 0) {
		while (*(line+i) != '\0' && *(line+i) != '\n') {
			if (*(line+i) >= 48 && *(line+i) <= 57) {
				c = (*(line+i++)-48);
			} else {
				c = *(line+i++);
				switch(c) {
					case '*':
						c = 0x0a;
						break;
					case '#':
						c = 0x0b;
				}

			}
			k = (c << 2) | ((((c >> 4) & 0x03) + ((c >> 2) & 0x03) + (c & 0x03)) & 0x03);
			ESP_LOGI(TAG, "Write %d [%x] to KeyBus\n", c, k);
      xQueueSend(write_queue, &k, NULL);
    }
  } else if (strncmp(line, "BIN", strlen("BIN")) == 0) {
		ESP_LOGW(TAG, "Switching to binary protocol\n");
		binary_mode = true;
	} else if (strncmp(line, "TEXT", strlen("TEXT")) == 0) {
		ESP_LOGW(TAG, "Switching to text protocol\n");
		binary_mode = false;
  }
}
void dsc_tcp_task(void *pvParameters) {
  char * dsc_server = dsc_config.dscserver;
  int port = dsc_config.port;
  char s_port[16];
  char recv_buf[DSC_TCP_RECV_BUF_SIZE];
  char line[256];
  const struct addrinfo hints = {
    .ai_family = AF_INET,
    .ai_socktype = SOCK_STREAM,
  };
  struct addrinfo *res;
  struct in_addr *addr;
  int r = NULL, buf_idx = 0;
  char *pstr, *pend;
  keybus_msg_t msg;

  sprintf(s_port, "%d", port);

  while (true) {
    xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT,
                           false, true, portMAX_DELAY);
    ESP_LOGI(TAG, "Connected to AP, trying to connect to DSC TCP server");
    int err = getaddrinfo(dsc_server, s_port, &hints, &res);
    if (err) {
      ESP_LOGE(TAG, "DNS lookup failed: %s", dsc_server);
      continue;
    }
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
    receiving_timeout.tv_sec = 1;
    receiving_timeout.tv_usec = 0;
    if (setsockopt(dsc_socket, SOL_SOCKET, SO_RCVTIMEO, &receiving_timeout, sizeof(receiving_timeout)) < 0) {
      ESP_LOGE(TAG, "... failed to set socket receiving timeout");
      close(dsc_socket);
      vTaskDelay(4000 / portTICK_PERIOD_MS);
      connected = false;
      continue;
    }
    char *banner = "DSC: ESP32 Keybus Gateway [denver@bitfire.co.za] v" VERSION "\n";
    dsc_tcp_write(dsc_socket, banner, strlen(banner));
    connected = true;
    bzero(line, sizeof(line));
    while (connected) {
      bzero(recv_buf, sizeof(recv_buf));
      r = read(dsc_socket, recv_buf+buf_idx, sizeof(recv_buf)-buf_idx);
      if (r > 0) {
        ESP_LOGD(TAG, "Read: %s", recv_buf);
        buf_idx += r;
        if (buf_idx >= DSC_TCP_RECV_BUF_SIZE) {
          ESP_LOGE(TAG, "Receive buffer overflow!");
          buf_idx = 0;
        }
        pstr = recv_buf;
        while ((pend = strchr(pstr, '\n')) != NULL) {
          if (*(pend-1) == '\r') *(pend-1) = '\0';
          memcpy(line, pstr, pend-pstr);
          dsc_tcp_handle_msg(line);
          pstr = pend+1;
          bzero(line, DSC_TCP_RECV_BUF_SIZE);
        }
        int rem = buf_idx-(pstr-recv_buf);
        if (pstr > recv_buf && pstr <= recv_buf+buf_idx) {
          memcpy(recv_buf, pstr, rem);
          bzero(recv_buf+rem, DSC_TCP_RECV_BUF_SIZE-rem);
          buf_idx = rem;
        }

      } else {
        if (errno == EAGAIN || errno == ETIMEDOUT) {
          //ESP_LOGD(TAG, "Socket errno: %d", errno);
          //continue;
        } else {
          ESP_LOGW(TAG, "Socket error: %d", errno);
          connected = false;
        }
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
