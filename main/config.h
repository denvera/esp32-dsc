#define VERSION "1.0.3"

#define USER_GPIO GPIO_NUM_0
#define LED_GPIO GPIO_NUM_2

#define LWIP_HTTPD_CGI_SSI 1
#define LWIP_HTTPD_SSI 1
#define LWIP_HTTPD_CGI 1
#define LWIP_HTTPD_SUPPORT_POST 1

#define RESET_FACTORY 1

void config_task(void *pvParameter);
