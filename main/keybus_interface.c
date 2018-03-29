#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#include "sdkconfig.h"

#include "driver/timer.h"

#include "config.h"
#include "keybus_interface.h"
#include "keybus_handler.h"

char panel_msg[128], periph_msg[128];
short bit_count = 0;
short byte_index = 0;
char last_clk = 0;
bool in_msg = false;



static void inline print_timer_counter(uint64_t counter_value)
{
    printf("Counter: 0x%08x%08x\n", (uint32_t) (counter_value >> 32),
                                    (uint32_t) (counter_value));
    printf("Time   : %.8f s\n", (double) counter_value / KEYBUS_TIMER_SCALE);
}

void keybus_init() {
  msg_queue = xQueueCreate(32, sizeof(keybus_msg_t));
  keybus_setup_timer();
  keybus_setup_gpio();
  //timer_start(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX);
}

static void IRAM_ATTR keybus_clock_isr_handler(void* arg)
{
    // check queue, read byte into static, write with bitcount
    if (!in_msg) {
      byte_index = 0;
      in_msg = true;
      //uint32_t gpio_num = (uint32_t) arg;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
      TIMERG0.int_clr_timers.t0 = 1;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].alarm_high = (uint32_t) 0;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].alarm_low = (uint32_t) KEYBUS_START_OFFSET;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 1;
    }
}

void IRAM_ATTR keybus_timer_isr(void *p) {
  // Read a bit

  char clock, data;
  uint32_t intr_status = TIMERG0.int_st_timers.val;
  TIMERG0.hw_timer[KEYBUS_TIMER_IDX].update = 1;
  uint64_t timer_counter_value =
        ((uint64_t) TIMERG0.hw_timer[KEYBUS_TIMER_IDX].cnt_high) << 32
        | TIMERG0.hw_timer[KEYBUS_TIMER_IDX].cnt_low;

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
    TIMERG0.int_clr_timers.t0 = 1;
    if (bit_count == 1) {
      //byte_index = 0;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
      TIMERG0.int_clr_timers.t0 = 1;
      //TIMERG0.hw_timer[KEYBUS_TIMER_IDX].alarm_high = (uint32_t) (KEYBUS_BIT_TIME >> 32);
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].alarm_high = (uint32_t) 0;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].alarm_low = (uint32_t) KEYBUS_BIT_TIME;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 1;

    } else if ((in_msg) && ((bit_count >= 255) || clock == last_clk)) { // Pause timer
      keybus_msg_t msg;
      TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.enable = 0;
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
    TIMERG0.hw_timer[KEYBUS_TIMER_IDX].config.alarm_en = TIMER_ALARM_EN;
    last_clk = clock;
    //if (clock) panel_msg[byte_index] <<= 1;
  }
}

void keybus_start_msg() {
  in_msg = false;
  timer_set_counter_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, 0x00000000ULL);
  timer_set_alarm_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, KEYBUS_START_OFFSET);

}

void keybus_reset_timer() { // Zero timer
  timer_set_counter_value(KEYBUS_TIMER_GROUP, KEYBUS_TIMER_IDX, 0x00000000ULL);
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
   io_conf.pull_up_en = 1;
   //configure GPIO with the given settings
   gpio_config(&io_conf);
   gpio_set_intr_type(KEYBUS_CLOCK, GPIO_INTR_POSEDGE);
   gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
   gpio_isr_handler_add(KEYBUS_CLOCK, keybus_clock_isr_handler, (void*) KEYBUS_CLOCK);
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
