#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include "esp_http_server.h"
#include "esp_timer.h"
#include "freertos/semphr.h"

httpd_handle_t start_webserver();
void stop_webserver(httpd_handle_t server);
void status_check_timer_callback(void *args);
void state_change_timer_callback(void *args);


extern SemaphoreHandle_t mutex;
extern esp_timer_handle_t status_timer;
extern esp_timer_handle_t state_timer;
#endif