#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include "device_actions.h"

httpd_handle_t start_webserver(void);
void stop_webserver(httpd_handle_t server);

extern SemaphoreHandle_t mutex;

#endif // WEB_SERVER_H
