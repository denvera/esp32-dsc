#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "config.h"

#include "keybus_handler.h"

char last_msg[128], last_periph_msg[128];
char formatted_msg[128];
static bool monitor_mode = false;

xQueueHandle msg_queue, write_queue, response_queue = NULL;

int format_msg(char msg[], int msg_len, char * outbuf) {
  memset(outbuf, 0, 3*msg_len);
  int outlen = 0, ret;
  for (int i = 0; i < msg_len; i++) {
    ret = snprintf(outbuf+(i*3), 4, "%02X ", msg[i]);
    if (ret >= 0) {
      outlen += ret;
    } else {
      return ret;
    }
  }
  return outlen;
}

void keybus_handler_task(void *pvParameter) {
  response_queue = xQueueCreate(32, sizeof(keybus_msg_t)); // Swiched to dispatch function
  gpio_pad_select_gpio(LED_GPIO);
/* Set the GPIO as a push/pull output */
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  unsigned char led = 0;
  bool new_msg = false;
  memset(last_msg, 0, 128);
  memset(last_periph_msg, 0, 128);
  memset(formatted_msg, 0, 128);

  while(1) {
      keybus_msg_t msg;
      xQueueReceive(msg_queue, &msg, portMAX_DELAY);
      led ^= 1;
      gpio_set_level(LED_GPIO, led);
      //printf("%x %x %x %x %x\n", msg.msg[0], msg.msg[1], msg.msg[2], msg.msg[3], msg.msg[4]);
      if (memcmp(msg.msg, last_msg, msg.len_bytes) != 0) {
        new_msg = true;
        if (monitor_mode) {
          //printf("\n%-10s%lld Bits: %d Bytes: %d\t", "Panel: ", msg.timer_counter_value, msg.len_bits, msg.len_bytes);
          for (int i = 0; i < msg.len_bytes; i++)
            snprintf(formatted_msg+(i*3), 4, "%02X ", msg.msg[i]);
          printf("\n%-10s Bits: %d Bytes: %d\t%-48s\t%s", "Panel: ", msg.len_bits, msg.len_bytes, formatted_msg, (keybus_handler_check_crc(msg.msg, msg.len_bytes) <= 0) ? "[OK]" : "[BAD]");
          //printf("\t%s", (keybus_handler_check_crc(msg.msg, msg.len_bytes) <= 0) ? "[OK]" : "[BAD]");
        }
        memcpy(last_msg, msg.msg, msg.len_bytes);
      }
      if (periph_msg_present(msg.pmsg, msg.len_bytes)) {
        new_msg = true;
        if (monitor_mode) {
          //printf("\n%-10s%lld Bits: %d Bytes: %d\t", "Periph: ", msg.timer_counter_value, msg.len_bits, msg.len_bytes);
          //for (int i = 0; i < msg.len_bytes; i++) printf("%02X ", msg.pmsg[i]);
          for (int i = 0; i < msg.len_bytes; i++)
            snprintf(formatted_msg+(i*3), 4, "%02X ", msg.pmsg[i]);
          const char * key_check = (msg.msg[0] == 0x11) ? "||]" : "BAD]";
          printf("\n%-10s Bits: %d Bytes: %d\t%-48s\t[%02X - %s", "Periph: ", msg.len_bits, msg.len_bytes, formatted_msg, (msg.pmsg[2] >> 2), (keybus_key_crc_ok(msg.pmsg[2]) ? "OK]" : key_check));
          //printf("\t%s", (keybus_key_crc_ok(msg.pmsg[2]) ? "[OK]" : key_check));
        }
        memcpy(last_periph_msg, msg.pmsg, msg.len_bytes);
      }
      if (new_msg) {
        dsc_config.dispatch_msg(msg);
        new_msg = false;
      }
      //in_msg = false;
      //timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
  }
}

void toggle_monitor_mode() {
  printf("Monitor mode %s\n", (monitor_mode == false) ? "on" : "off");
  monitor_mode = (monitor_mode) ? false : true;
}

bool periph_msg_present(char periph_msg[], int len) {
  if (len < 3) return false;
  for (int i = 2; i < len-1; i++) {
    if (periph_msg[i] != 0xff) return true;
  }
  return false;
}

bool keybus_key_crc_ok(char k) {
  return ((((k >> 6) & 0x03) + ((k >> 4) & 0x03) + ((k >> 2) & 0x03)) & 0x03) == (k & 0x03);
}

// 0 if ok, 1 crc error, -1 no crc
int keybus_handler_check_crc(char *msg, char len) {
  int bytes = len-2;
  int crc = msg[len-2];
  switch (msg[0]) {
      case 0x05:
      case 0x11:
        return -1;
      default:
        while (bytes--)
          crc -= msg[bytes];
        return crc & 0xff;
  }
}

char keybus_add_crc(char c) {
  switch (c) {
    case '*':
      c = 0x0a;
      break;
    case '#':
      c = 0x0b;
      break;
  }
  return ((c << 2) | ((((c >> 4) & 0x03) + ((c >> 2) & 0x03) + (c & 0x03)) & 0x03));
}
