#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#include "driver/timer.h"

#include "config.h"
#include "keybus.h"

char panel_msg[128], periph_msg[128];
char bit_count = 0;
char last_clk = 0;
bool in_msg = false;

xQueueHandle msg_queue;

typedef struct {
    char msg[KEYBUS_MSG_SIZE];
    uint64_t timer_counter_value;
} keybus_msg_t;

static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / KEYBUS_TIMER_SCALE);
}

void keybus_init() {
  msg_queue = xQueueCreate(32, sizeof(keybus_msg_t));
  keybus_setup_timer();

  in_msg = true;
  timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
}

void keybus_task(void *pvParameter) {
  gpio_pad_select_gpio(LED_GPIO);
/* Set the GPIO as a push/pull output */
  gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
  unsigned char led = 0;
  while(1) {
      keybus_msg_t msg;
      xQueueReceive(msg_queue, &msg, portMAX_DELAY);
      led ^= 1;
      gpio_set_level(LED_GPIO, led);
      printf("Received msg: %lld Bits: %d\n", msg.timer_counter_value, bit_count);
      print_timer_counter(msg.timer_counter_value);
      printf("Task values\n");
      uint64_t task_counter_value;
      timer_get_counter_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, &task_counter_value);
      print_timer_counter(task_counter_value);
      bit_count = 0;
      reset_timer();
      timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
  }
}

void IRAM_ATTR keybus_timer_isr(void *p) {
  // Read a bit
  bit_count++;
  uint32_t intr_status = TIMERG0.int_st_timers.val;
  TIMERG0.hw_timer[KEYBUS_TIMER_IDX].update = 1;
  uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[KEYBUS_TIMER_IDX].cnt_high) << 32
        | TIMERG0.hw_timer[KEYBUS_TIMER_IDX].cnt_low;
  // Clear interrupt
  TIMERG0.int_clr_timers.t0 = 1;
  TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.alarm_en = TIMER_ALARM_EN;
  if (intr_status & BIT(KEYBUS_TIMER_IDX)) {
    if ((!in_msg) || (bit_count >= 255)) { // Pause timer
      keybus_msg_t msg;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
      msg.timer_counter_value = timer_counter_value;
      // for (int i = 0; i < KEYBUS_MSG_SIZE; i++)
      //   msg.msg[i] = panel_msg[i];
      xQueueSendFromISR(msg_queue, &msg, NULL);
    } else {

    }
  }
}

void reset_timer() { // Zero timer
  timer_set_counter_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, 0x00000000ULL);
}

void keybus_setup_timer() {
  printf("Timer Setup:\nTIMER_BASE_CLK:\t\t%d\nKEYBUS_BIT_TIME:\t%d\n", TIMER_BASE_CLK, KEYBUS_BIT_TIME);
  timer_config_t config;
  config.divider = KEYBUS_TIMER_DIVIDER;
  config.counter_dir = TIMER_COUNT_UP;
  config.counter_en = TIMER_PAUSE;
  config.alarm_en = TIMER_ALARM_EN;
  config.intr_type = TIMER_INTR_LEVEL;
  config.auto_reload = 1;
  timer_init(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, &config);

  /* Timer's counter will initially start from value below.
     Also, if auto_reload is set, this value will be automatically reload on alarm */
  timer_set_counter_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, 0x00000000ULL);

  /* Configure the alarm value and the interrupt on alarm. */
  timer_set_alarm_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, KEYBUS_BIT_TIME);
  timer_enable_intr(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
  timer_isr_register(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, keybus_timer_isr,
      (void *) KEYBUS_TIMER_IDX, ESP_INTR_FLAG_IRAM, NULL);

  // /timer_start(TIMER_GROUP_0, timer_idx);
}
