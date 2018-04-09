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

static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / KEYBUS_TIMER_SCALE);
}

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
  // Below started in keybus_write_task to ensure ISRs sit on Core 1 (App)
  // keybus_setup_timer();
  // keybus_setup_gpio();
  //setup_gpio_timers(NULL);
  //timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
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
  if (write_sem == NULL) {
    ESP_LOGE(TAG, "Couldn't create write semaphore!");
  }
  char b;
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

void IRAM_ATTR keybus_timer_isr(void *p) {
  // Read a bit

  char clock, data;
  uint32_t intr_status = TIMERG1.int_st_timers.val;
  TIMERG1.hw_timer[KEYBUS_TIMER_IDX].update = 1;
  uint64_t timer_counter_value =
        ((uint64_t) TIMERG1.hw_timer[KEYBUS_TIMER_IDX].cnt_high) << 32
        | TIMERG1.hw_timer[KEYBUS_TIMER_IDX].cnt_low;

  if (intr_status & BIT(KEYBUS_TIMER_IDX)) {
    clock = ((GPIO.in >> KEYBUS_CLOCK) & 0x1);
    data = ((GPIO.in >> KEYBUS_DATA) & 0x1);
    if (clock && ((last_clk == 0) || (bit_count == 0))) { // Panel to periphs
      bit_count++;
      panel_msg[byte_index] = (panel_msg[byte_index] << 1) | data;
      //panel_msg[byte_index] |= data;
      if (((bit_count == 8) || ((bit_count == 9)) || ((bit_count > 9) && ((bit_count-9) % 8 == 0))) && (byte_index <= KEYBUS_MSG_SIZE))  byte_index++;
    } else { // Periphs to panel
      periph_msg[byte_index] = (periph_msg[byte_index] << 1) | data;
    }
    // Clear interrupt
    TIMERG1.int_clr_timers.t0 = 1;
    if (bit_count == 1) {
      //byte_index = 0;
      TIMERG1.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
      TIMERG1.int_clr_timers.t0 = 1;
      //TIMERG1.hw_timer[KEYBUS_TIMER_IDX].alarm_high = (uint32_t) (KEYBUS_BIT_TIME >> 32);
      TIMERG1.hw_timer[KEYBUS_TIMER_IDX].alarm_high = (uint32_t) 0;
      TIMERG1.hw_timer[KEYBUS_TIMER_IDX].alarm_low = (uint32_t) KEYBUS_BIT_TIME;
      TIMERG1.hw_timer[KEYBUS_TIMER_IDX].config.enable = 1;

    } else if ((in_msg) && ((bit_count >= 255) || clock == last_clk)) { // Pause timer
      keybus_msg_t msg;
      TIMERG1.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
      msg.timer_counter_value = timer_counter_value;
      msg.len_bits = bit_count;
      msg.len_bytes = byte_index + 1;
      //for (int i = 0; i <= byte_index; i++)
      //   msg.msg[i] = panel_msg[i];
      memcpy(msg.msg, panel_msg, byte_index+1);
      memcpy(msg.pmsg, periph_msg, byte_index+1);
      if (bit_count >= 32) {
        xQueueSendFromISR(msg_queue, &msg, NULL);
      } else {
        in_msg = false;
        bit_count = 0;
        byte_index = 0;
      }
      in_msg = false;
      bit_count = 0;
      byte_index = 0;
      memset(panel_msg, 0, 128);
      memset(periph_msg, 0, 128);
    }
    TIMERG1.hw_timer[KEYBUS_TIMER_IDX].config.alarm_en = TIMER_ALARM_EN;
    last_clk = clock;
    //if (clock) panel_msg[byte_index] <<= 1;
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
