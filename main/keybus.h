#include "driver/gpio.h"

#define KEYBUS_DATA 4
#define KEYBUS_CLOCK 5

#define KEYBUS_MSG_SIZE 128


#define KEYBUS_TIMER_DIVIDER 8
#define KEYBUS_TIMER_IDX TIMER_0
#define KEYBUS_TIMER_GROUP TIMER_GROUP_0
#define KEYBUS_TIMER_SCALE (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER)
//#define KEYBUS_BIT_TIME (0.0001 * (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER))
#define KEYBUS_BIT_TIME ((int)(0.0005 * (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER)))
#define KEYBUS_START_OFFSET ((int)(0.00025 * (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER)))
//#define KEYBUS_MSG_DELAY 200
#define ESP_INTR_FLAG_DEFAULT 0


void keybus_init();
void keybus_reset_timer();
void keybus_start_msg();
void keybus_setup_gpio();
void keybus_task(void *pvParameter);
void keybus_setup_timer();
int keybus_check_crc(char *msg, char len);

void IRAM_ATTR keybus_timer_isr(void *p);
