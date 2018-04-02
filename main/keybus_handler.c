#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sdkconfig.h"
#include "config.h"

#include "keybus_handler.h"

char last_msg[128], last_periph_msg[128];
static bool monitor_mode = false;

void keybus_handler_task(void *pvParameter) {
  gpio_pad_select_gpio(LED_GPIO);
/* Set the GPIO as a push/pull output */
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  unsigned char led = 0;
  memset(last_msg, 0, 128);
  memset(last_periph_msg, 0, 128);
  while(1) {
      keybus_msg_t msg;
      xQueueReceive(msg_queue, &msg, portMAX_DELAY);
      led ^= 1;
      gpio_set_level(LED_GPIO, led);
      //printf("%x %x %x %x %x\n", msg.msg[0], msg.msg[1], msg.msg[2], msg.msg[3], msg.msg[4]);
      if (memcmp(msg.msg, last_msg, msg.len_bytes) != 0) {
        if (monitor_mode) {
          printf("\n%lld Bits: %d Bytes: %d\t\t", msg.timer_counter_value, msg.len_bits, msg.len_bytes);
          for (int i = 0; i < msg.len_bytes; i++) printf("%02X ", msg.msg[i]);
          printf("\t%s", (keybus_handler_check_crc(msg.msg, msg.len_bytes) <= 0) ? "[OK]" : "[BAD]");
        }
        memcpy(last_msg, msg.msg, msg.len_bytes);
      }
      if (memcmp(msg.pmsg, last_periph_msg, msg.len_bytes) != 0) {
        if (monitor_mode) {
          printf("\nPeripheral: %lld Bits: %d Bytes: %d\t\t", msg.timer_counter_value, msg.len_bits, msg.len_bytes);
          for (int i = 0; i < msg.len_bytes; i++) printf("%02X ", msg.pmsg[i]);
          printf("\t%s", (keybus_handler_check_crc(msg.pmsg, msg.len_bytes) <= 0) ? "[OK]" : "[BAD]");
        }
        memcpy(last_periph_msg, msg.pmsg, msg.len_bytes);
      }

      //in_msg = false;
      //timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
  }
}

void toggle_monitor_mode() {
  printf("Monitor mode %s\n", (monitor_mode == false) ? "on" : "off");
  monitor_mode = (monitor_mode) ? false : true;
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
