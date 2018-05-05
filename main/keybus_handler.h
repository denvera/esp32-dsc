#ifndef KEYBUS_HANDLER_H
#define KEYBUS_HANDLER_H

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

extern xQueueHandle msg_queue, write_queue, response_queue;

int format_msg(char msg[], int msg_len, char * outbuf);
void keybus_handler_task(void *pvParameter);
int keybus_handler_check_crc(char *msg, char len);
bool periph_msg_present(char periph_msg[], int len);
bool keybus_key_crc_ok(char key);
void toggle_monitor_mode();


#endif
