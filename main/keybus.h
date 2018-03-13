#include "driver/gpio.h"

#define SOME_STUFF

#define KEYBUS_MSG_SIZE 128


#define KEYBUS_TIMER_DIVIDER 16
#define KEYBUS_TIMER_IDX TIMER_0
#define KEYBUS_TIMER_GROUP TIMER_GROUP_0
#define KEYBUS_TIMER_SCALE (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER)
//#define KEYBUS_BIT_TIME (0.0001 * (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER))
#define KEYBUS_BIT_TIME ((int)(0.01 * (TIMER_BASE_CLK / KEYBUS_TIMER_DIVIDER)))
//#define KEYBUS_MSG_DELAY 200


void keybus_init();
void reset_timer();
void keybus_task(void *pvParameter);
void keybus_setup_timer();
