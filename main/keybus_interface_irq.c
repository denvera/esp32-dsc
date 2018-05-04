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
volatile uint32_t last_msg_time = 0;
volatile uint32_t last_dispatch_time = 0;
bool in_msg = false;
volatile bool write_byte_ready = false;
char write_byte;
volatile bool writing = false;

static TaskHandle_t keybus_write_task_handle = NULL;
SemaphoreHandle_t write_sem, last_msg_time_sem;


void keybus_setup_task(void *pvParameter) {
  last_msg_time_sem = xSemaphoreCreateBinary();
  keybus_setup_gpio();
  vTaskDelete( NULL );
}

void keybus_init() {
  msg_queue = xQueueCreate(32, sizeof(keybus_msg_t));
  write_queue = xQueueCreate(32, sizeof(char));
  ESP_LOGI(TAG, "KeyBus IRQ Initialisation");
  // BaseType_t task_ret = xTaskCreatePinnedToCore(&keybus_write_task, "keybus_write_task_app_cpu", 8192, NULL, 0, &keybus_write_task_handle, 0); // App CPU, Priority 10
  // if( task_ret != pdPASS ) {
  //   ESP_LOGE(TAG, "Couldn't create keybus_write_task!");
  // }
#ifdef CONFIG_KEYBUS_SNIFFING
  xTaskCreatePinnedToCore(&keybus_client_read_task, "keybus_client_read_task", 8192, NULL, 2, NULL, 0); // App CPU, Priority 10
#endif
  xTaskCreatePinnedToCore(&keybus_setup_task, "keybus_setup_task", 8192, NULL, (configMAX_PRIORITIES-1), NULL, 1); // App CPU, Priority 10
  xTaskCreatePinnedToCore(&keybus_stop_check_task, "keybus_stop_check_task", 8192, NULL, 2, NULL, 1); // App CPU, Priority 10
}

static void IRAM_ATTR keybus_clock_isr_handler(void* arg)
{
  uint32_t gpio_num = (uint32_t) arg;
  //static bool writing = false;
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
      //memset(msg.pmsg, 0xff, 128);
      xQueueSendFromISR(msg_queue, &msg, NULL);
      //memset(msg.msg, 0xff, 128);
      last_dispatch_time = xthal_get_ccount();
    }
    byte_index = 0;
    bit_count = 0;
    msg.msg[0] = 0;
    }
    if (clock) {
      // Run another background task to check last_clk time now and then, and set back to input if its too long
      //if (writing)
      gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT);

      bit_count++;
      msg.msg[byte_index] = (msg.msg[byte_index] << 1) | data;
      if (((bit_count == 8) || ((bit_count == 9)) || ((bit_count > 9) && ((bit_count-9) % 8 == 0))) && (byte_index <= KEYBUS_MSG_SIZE))  {
        byte_index++;
        msg.msg[byte_index] = 0; //Set next byte used to 0 to avoid having to do a memset above
        msg.pmsg[byte_index] = 0;
      }
    } else {
      //if (byte_index == 2 && write_byte_ready && msg.msg[0] == 0x05) { //write coming in mid 2nd byte?
      if (byte_index == 2 && (bit_count <= 9) && msg.msg[0] == 0x05) { //write coming in mid 2nd byte?
      //if (byte_index == 2 && (xSemaphoreTakeFromISR(write_sem, NULL) == pdTRUE) && msg.msg[0] == 0x05) {
        if (xQueueReceiveFromISR(write_queue, &write_byte, NULL) == pdTRUE) {
          gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT_OUTPUT);
          writing = true;
          write_byte_ready = false;
        }
      }
      if (writing) {
        if (byte_index > 2) {
          gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT);
          writing = false;
          //xSemaphoreGiveFromISR(write_sem, NULL);
        } else {
          gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT_OUTPUT);
          gpio_set_level(KEYBUS_DATA, ((write_byte >> (16-bit_count)) & 0x01));
        }
      }
#ifdef CONFIG_KEYBUS_SNIFFING
      // Snoop - We need to give the peripheral some time to react to the low clock, see below :(
      for (volatile int i = 0; i<=240*10; i++) { }; // This is disgusting :(( (need around 20-50uS delay)
      msg.pmsg[byte_index] = (msg.pmsg[byte_index] << 1) | ((GPIO.in >> KEYBUS_DATA) & 0x1);
#endif
    }
    //xSemaphoreTakeFromISR(last_msg_time_sem, NULL);
    last_msg_time = (xthal_get_ccount());
    //xSemaphoreGiveFromISR(last_msg_time_sem, NULL);
  }
}

void keybus_write_task(void *pvParameter) {
  write_sem = xSemaphoreCreateBinary();
  char b;
  if (write_sem == NULL) {
    ESP_LOGE(TAG, "Couldn't create write semaphore!");
  }
  ESP_LOGI(TAG, "keybus_write_task started");
  for(;;) {
      //xQueueReceive(write_queue, &b, portMAX_DELAY);
      //write_byte = b;
      //write_byte_ready = true;
      //xSemaphoreGive(write_sem);
      //ESP_LOGI(TAG, "Writing: %02X", b);
      xSemaphoreTake(write_sem, portMAX_DELAY);
  }
}

void keybus_client_read_task(void *pvParameter) {
  ESP_LOGI(TAG, "KeyBus client read task started");
  for (;;) {
    vTaskDelay(5000 / portTICK_RATE_MS);
  }
}

void keybus_stop_check_task(void *pvParameter) {
  ESP_LOGI(TAG, "KeyBus stop check task started");
  int failcheck = 0;
  for (;;) {
    vTaskDelay(10 / portTICK_RATE_MS);
    //xSemaphoreTake(last_msg_time_sem, portMAX_DELAY);
    if ((XTHAL_GET_CCOUNT() - last_dispatch_time) > (250 * CONFIG_KEYBUS_WAIT_US * CONFIG_ESP32_DEFAULT_CPU_FREQ_MHZ)) {
      if (++failcheck >= 3) {
        ESP_LOGW(TAG, "KeyBus clk stopped - switching to input!");
        gpio_set_direction(KEYBUS_DATA, GPIO_MODE_INPUT);
        if (writing) {
          writing = false;
          write_byte_ready = false;
          //xSemaphoreGive(write_sem);
        }
        failcheck = 0;
      }
    }
    //xSemaphoreGive(last_msg_time_sem);
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
