#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "sdkconfig.h"
#include "driver/timer.h"
#include "esp_log.h"

#include "xtensa/core-macros.h"


#include "config.h"
#include "keybus_interface_irq.h"
#include "keybus_handler.h"

static const char* TAG = "keybus_interface_irq";

char panel_msg[128], periph_msg[128];
short bit_count = 0;
short byte_index = 0;
char last_clk = 0;
uint32_t last_msg_time = 0;
bool in_msg = false;
bool write_byte_ready = false;
char write_byte;


static TaskHandle_t keybus_write_task_handle = NULL;
SemaphoreHandle_t write_sem;


void setup_gpio_timers(void *pvParameter) {
  keybus_setup_gpio();
  vTaskDelete( NULL );
}

void keybus_init() {
  msg_queue = xQueueCreate(32, sizeof(keybus_msg_t));
  write_queue = xQueueCreate(32, sizeof(char));
  ESP_LOGI(TAG, "KeyBus IRQ Initialisation");
  BaseType_t task_ret = xTaskCreatePinnedToCore(&keybus_write_task, "keybus_write_task_app_cpu", 8192, NULL, (configMAX_PRIORITIES-1), &keybus_write_task_handle, 0); // App CPU, Priority 10
  if( task_ret != pdPASS ) {
    ESP_LOGE(TAG, "Couldn't create keybus_write_task!");
  }
  xTaskCreatePinnedToCore(&setup_gpio_timers, "setup_gpio_timers", 8192, NULL, (configMAX_PRIORITIES-1), NULL, 1); // App CPU, Priority 10
}

static void IRAM_ATTR keybus_clock_isr_handler(void* arg)
{
  uint32_t gpio_num = (uint32_t) arg;
  static bool writing = false;
  // check queue, read byte into static, write with bitcount
  char clock = (GPIO.in >> KEYBUS_CLOCK) & 0x1;
  char data = ((GPIO.in >> KEYBUS_DATA) & 0x1);
  static keybus_msg_t msg;
  if (gpio_num == KEYBUS_CLOCK) { // Not sure if required

  //xTaskNotifyFromISR(keybus_write_task_handle, 0x00, eNoAction, NULL);
  if ((xthal_get_ccount() - last_msg_time) > (CONFIG_KEYBUS_WAIT_US * CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ)) {
    // fire off previous message
    if (byte_index > 2) {
      msg.len_bytes = byte_index+1;
      memset(msg.pmsg, 0xff, 128);
      xQueueSendFromISR(msg_queue, &msg, NULL);
      //memset(msg.msg, 0xff, 128);
    }
    byte_index = 0;
    bit_count = 0;
    msg.msg[0] = 0;
    }
    if (clock) {
      bit_count++;
      msg.msg[byte_index] = (msg.msg[byte_index] << 1) | data;
      if (((bit_count == 8) || ((bit_count == 9)) || ((bit_count > 9) && ((bit_count-9) % 8 == 0))) && (byte_index <= KEYBUS_MSG_SIZE))  {
        byte_index++;
        msg.msg[byte_index] = 0;
      }
    } else {
      if (byte_index == 2 && write_byte_ready && msg.msg[0] == 0x05) {
      //if (byte_index == 2 && (xSemaphoreTakeFromISR(write_sem, NULL) == pdTRUE) && msg.msg[0] == 0x05) {
        gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT_OUTPUT);
        writing = true;
        write_byte_ready = false;
      }
      if (writing) {
        if (byte_index > 2) {
          gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT);
          writing = false;
          xSemaphoreGiveFromISR(write_sem, NULL);
        } else {
          gpio_set_level(KEYBUS_DATA, ((write_byte >> (16-bit_count)) & 0x01));
        }
      }
    }
    last_msg_time = (xthal_get_ccount());
  }
}

void keybus_write_task(void *pvParameter) {
  write_sem = xSemaphoreCreateBinary();
  char b;
  if (write_sem == NULL) {
    ESP_LOGE(TAG, "Couldn't create write semaphore!");
  }
  ESP_LOGI(TAG, "keybus_write_task started");
  for( ;; ) {
      xQueueReceive(write_queue, &b, portMAX_DELAY);
      write_byte = b;
      write_byte_ready = true;
      //xSemaphoreGive(write_sem);
      ESP_LOGI(TAG, "Writing: %02X", b);
      xSemaphoreTake(write_sem, portMAX_DELAY);
  }
}

void keybus_setup_gpio() {
   gpio_config_t io_conf;
   io_conf.intr_type = GPIO_INTR_DISABLE;
   //set as output mode
   io_conf.mode = GPIO_MODE_INPUT;
   io_conf.pin_bit_mask = (1 << KEYBUS_DATA | 1 << KEYBUS_CLOCK);
   //disable pull-down mode
   io_conf.pull_down_en = 0;
   //disable pull-up mode
   io_conf.pull_up_en = 0;
   //configure GPIO with the given settings
   gpio_config(&io_conf);
   gpio_set_intr_type(KEYBUS_CLOCK, GPIO_INTR_ANYEDGE);
   gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
   gpio_isr_handler_add(KEYBUS_CLOCK, keybus_clock_isr_handler, (void*) KEYBUS_CLOCK);
}
