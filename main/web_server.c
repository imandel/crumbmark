#include "web_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "smart_plug_proxy";

SemaphoreHandle_t mutex;
esp_timer_handle_t status_timer;
esp_timer_handle_t state_timer;

// Plug state (0 for off, 1 for on)
static int plug_state = 0;
// Status information
static const char *firmware_version = "1.0.0";

// Utility function to create JSON objects
cJSON *create_json_status() {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "firmware", firmware_version);
    return root;
}

cJSON *create_json_state() {
cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "state", plug_state);
    return root;
}


// Status handler
esp_err_t status_get_handler(httpd_req_t *req) {
cJSON *status_json = create_json_status();
    char *json_str = cJSON_Print(status_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
cJSON_Delete(status_json);
    cJSON_free(json_str);
return ESP_OK;
}

// State GET handler
esp_err_t state_get_handler(httpd_req_t *req) {
    cJSON *state_json = create_json_state();
    char *json_str = cJSON_Print(state_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    cJSON_Delete(state_json);
    cJSON_free(json_str);
    return ESP_OK;
}


// State PUT handler
esp_err_t state_put_handler(httpd_req_t *req) {
    char buf[100];
    int ret;

    /* Read request content */
    ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
             httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }


    cJSON *root = cJSON_Parse(buf);
    if (root == NULL){
         httpd_resp_send_408(req);
    } else {
        cJSON *state_item = cJSON_GetObjectItem(root, "state");
        if (state_item != NULL && cJSON_IsNumber(state_item)) {
        // Protect the plug state shared variable with mutex
        xSemaphoreTake(mutex, portMAX_DELAY);
            plug_state = state_item->valueint;
            xSemaphoreGive(mutex);
            cJSON_Delete(root);

            // Simulate a small delay
            vTaskDelay(pdMS_TO_TICKS(10));
            // httpd_resp_send(req, "state updated", HTTPD_RESP_AUTO);
            httpd_resp_send_408(req);
        } else{
            httpd_resp_send_408(req);
        }
    }
return ESP_OK;
}

// Handler for root URL
esp_err_t root_handler(httpd_req_t *req) {
    char html[512];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html><html><body>"
        "<h1>Hello World</h1>"
        "<p>Current plug state: %s</p>"
        "</body></html>",
        plug_state ? "ON" : "OFF"
    );
    
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, strlen(html));
    return ESP_OK;
}

// URI handlers
httpd_uri_t root_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_handler,
    .user_ctx = NULL
};

httpd_uri_t status_uri = {
    .uri = "/plug/status",
    .method = HTTP_GET,
    .handler = status_get_handler,
    .user_ctx = NULL};

httpd_uri_t state_get_uri = {
    .uri = "/plug/state",
    .method = HTTP_GET,
    .handler = state_get_handler,
    .user_ctx = NULL};

httpd_uri_t state_put_uri = {
    .uri = "/plug/state",
    .method = HTTP_PUT,
    .handler = state_put_handler,
    .user_ctx = NULL};


// Helper function to start the server
httpd_handle_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG, "Webserver started on port 80");
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &state_get_uri);
        httpd_register_uri_handler(server, &state_put_uri);
        return server;
    }
    ESP_LOGE(TAG, "Failed to start webserver");
return NULL;
}

void stop_webserver(httpd_handle_t server) {
    if (server) {
        httpd_stop(server);
        ESP_LOGI(TAG, "Webserver stopped");
    }
}

// Simulate frequent status checks with a timer
void status_check_timer_callback(void *args) {
    esp_http_client_config_t config = {
        .url = "http://192.168.1.100/plug/status",
    };
esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Status request success");
    } else {
        ESP_LOGE(TAG, "Status request failed %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}

// Simulate state change with a timer
void state_change_timer_callback(void *args) {
// Protect the plug state shared variable with mutex
    xSemaphoreTake(mutex, portMAX_DELAY);
    plug_state = (plug_state + 1) % 2;
    xSemaphoreGive(mutex);
    char json_payload[30];
    sprintf(json_payload, "{\"state\": %d}", plug_state);
    esp_http_client_config_t config = {
        .url = "http://192.168.1.100/plug/state",
        .method = HTTP_METHOD_PUT,
    };
esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_post_field(client, json_payload, strlen(json_payload));
	esp_http_client_set_header(client, "Content-Type", "application/json");
esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "State request success");
    } else {
        ESP_LOGE(TAG, "State request failed %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(client);
}
