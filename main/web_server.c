#include "web_server.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "core_main.h"
#include "nvs_flash.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static const char *TAG = "smart_plug_proxy";

// Structure to hold benchmark results
typedef struct {
    int iterations_per_sec;
    float total_time;
    int error_count;
    bool is_running;
} benchmark_result_t;

// Shared benchmark state
static benchmark_result_t benchmark_state = {
    .is_running = false
};
static SemaphoreHandle_t benchmark_mutex = NULL;

// Forward declaration and implementation of benchmark task
static void benchmark_task(void *pvParameters) {
    ee_printf("Starting CoreMark benchmark...\n");
    
    vTaskPrioritySet(NULL, configMAX_PRIORITIES - 3);
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    int64_t start_time = esp_timer_get_time();
    int error_count = coremark_main();
    float total_time = (esp_timer_get_time() - start_time) / 1000000.0;
    
    xSemaphoreTake(benchmark_mutex, portMAX_DELAY);
    benchmark_state.error_count = error_count;
    benchmark_state.total_time = total_time;
    benchmark_state.iterations_per_sec = (total_time > 0) ? (1000 / total_time) : 0;
    benchmark_state.is_running = false;
    xSemaphoreGive(benchmark_mutex);
    
    vTaskDelete(NULL);
}

// Add new endpoint to get benchmark results
esp_err_t benchmark_results_handler(httpd_req_t *req) {
    if (benchmark_mutex == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    xSemaphoreTake(benchmark_mutex, portMAX_DELAY);
    benchmark_result_t current_state = benchmark_state;
    xSemaphoreGive(benchmark_mutex);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "running", current_state.is_running);
    
    if (!current_state.is_running && current_state.total_time > 0) {
        cJSON_AddNumberToObject(root, "iterations_per_sec", current_state.iterations_per_sec);
        cJSON_AddNumberToObject(root, "total_time_seconds", current_state.total_time);
        cJSON_AddNumberToObject(root, "error_count", current_state.error_count);
    }

    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    cJSON_Delete(root);
    cJSON_free(json_str);
    return err;
}

esp_err_t benchmark_handler(httpd_req_t *req) {
    if (benchmark_mutex == NULL) {
        benchmark_mutex = xSemaphoreCreateMutex();
    }

    xSemaphoreTake(benchmark_mutex, portMAX_DELAY);
    bool already_running = benchmark_state.is_running;
    xSemaphoreGive(benchmark_mutex);

    if (already_running) {
        // Return current status if benchmark is already running
        cJSON *root = cJSON_CreateObject();
        cJSON_AddBoolToObject(root, "running", true);
        char *json_str = cJSON_Print(root);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "close");
        esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
        cJSON_Delete(root);
        cJSON_free(json_str);
        return err;
    }

    // Start new benchmark
    xSemaphoreTake(benchmark_mutex, portMAX_DELAY);
    benchmark_state.is_running = true;
    xSemaphoreGive(benchmark_mutex);
    
    xTaskCreate(benchmark_task, "benchmark", 8192, NULL, tskIDLE_PRIORITY + 1, NULL);
    
    // Return immediate response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddBoolToObject(root, "running", true);
    
    char *json_str = cJSON_Print(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    
    // Cleanup
    cJSON_Delete(root);
    cJSON_free(json_str);
    
    return err;
}

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
    httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
    cJSON_Delete(state_json);
    cJSON_free(json_str);
    return err;
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
    if (root == NULL) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    cJSON *state_item = cJSON_GetObjectItem(root, "state");
    if (state_item == NULL || !cJSON_IsNumber(state_item)) {
        cJSON_Delete(root);
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
        // Protect the plug state shared variable with mutex
        xSemaphoreTake(mutex, portMAX_DELAY);
        plug_state = state_item->valueint;
        xSemaphoreGive(mutex);
        
        // Create response
        cJSON *response = cJSON_CreateObject();
        cJSON_AddNumberToObject(response, "state", plug_state);
        char *json_str = cJSON_Print(response);
        
        // Send response
        httpd_resp_set_type(req, "application/json");
        httpd_resp_set_hdr(req, "Connection", "close");
        esp_err_t err = httpd_resp_send(req, json_str, strlen(json_str));
        
        // Cleanup
        cJSON_Delete(root);
        cJSON_Delete(response);
        cJSON_free(json_str);
        
        return err;
}

// Handler for root URL
esp_err_t root_handler(httpd_req_t *req) {
    char html[512];
    snprintf(html, sizeof(html),
        "<!DOCTYPE html>\n"
        "<html lang=\"en\">\n"
        "<head>\n"
        "    <meta charset=\"UTF-8\">\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
        "    <title>ESP8266 Web Server</title>\n"
        "</head>\n"
        "<body>\n"
        "    <h1>Hello World</h1>\n"
        "    <p>Current plug state: %s</p>\n"
        "</body>\n"
        "</html>",
        plug_state ? "ON" : "OFF"
    );
    
    httpd_resp_set_type(req, "text/html; charset=UTF-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, no-store, must-revalidate");
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
    config.lru_purge_enable = true;  // Enable LRU purge
    config.max_uri_handlers = 8;     // Increase max handlers

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
    ESP_LOGI(TAG, "Webserver started on port 80");
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &state_get_uri);
        httpd_register_uri_handler(server, &state_put_uri);
    
        httpd_uri_t benchmark_uri = {
            .uri = "/benchmark",
            .method = HTTP_GET,
            .handler = benchmark_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &benchmark_uri);

        httpd_uri_t benchmark_results_uri = {
            .uri = "/benchmark/results",
            .method = HTTP_GET,
            .handler = benchmark_results_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &benchmark_results_uri);
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
