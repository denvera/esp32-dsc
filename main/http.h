#ifndef HTTP_H
#define HTTP_H

#include "libesphttpd/route.h"
#include "libesphttpd/webpages-espfs.h"
#include "libesphttpd/espfs.h"
#include "libesphttpd/httpdespfs.h"
#include "libesphttpd/httpd-freertos.h"

CgiStatus ICACHE_FLASH_ATTR tplIndex(HttpdConnData *connData, char *token, void **arg);
CgiStatus ICACHE_FLASH_ATTR tplSettings(HttpdConnData *connData, char *token, void **arg);
CgiStatus ICACHE_FLASH_ATTR cgiReboot(HttpdConnData *connData);
CgiStatus ICACHE_FLASH_ATTR cgiSettings(HttpdConnData *connData);
void setup_httpd();


#endif
