#include "freertos/queue.h"

//#define KEYBUS_MSG_SIZE 128
#define KEYBUS_MSG_SIZE CONFIG_KEYBUS_MSG_SIZE

typedef struct {
    char msg[KEYBUS_MSG_SIZE];
    char pmsg[KEYBUS_MSG_SIZE];
    short len_bits;
    short len_bytes;
    uint64_t timer_counter_value;
} keybus_msg_t;

xQueueHandle msg_queue, write_queue;


void keybus_handler_task(void *pvParameter);
int keybus_handler_check_crc(char *msg, char len);
