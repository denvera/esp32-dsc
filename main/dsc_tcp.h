#ifndef DSC_TCP_H
#define DSC_TCP_H

#include "keybus_handler.h"

esp_err_t dsc_tcp_dispatch_msg(keybus_msg_t msg);
void dsc_tcp_task(void *pvParameters);

#endif
