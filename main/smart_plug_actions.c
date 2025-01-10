#include "device_actions.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

#define PLUG_PIN GPIO_NUM_5
static const char *TAG = "smart_plug";
static int plug_state = 0;

static action_response_t handle_get_state(const cJSON *request) {
    action_response_t response = {0};
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "state", plug_state);
    response.response = cJSON_Print(root);
    response.status = ESP_OK;
    cJSON_Delete(root);
    return response;
}

static action_response_t handle_put_state(const cJSON *request) {
    action_response_t response = {0};
    cJSON *state_item = cJSON_GetObjectItem(request, "state");
    
    if (cJSON_IsNumber(state_item)) {
        plug_state = state_item->valueint;
        gpio_set_level(PLUG_PIN, plug_state);
        response.status = ESP_OK;
        response.response = strdup("State updated");
    } else {
        response.status = ESP_ERR_INVALID_ARG;
        response.response = strdup("Invalid state value");
    }
    return response;
}

static esp_err_t plug_init(void) {
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PLUG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&io_conf);
}

static const device_endpoint_t plug_endpoints[] = {
    {
        .uri = "/state",
        .method = ACTION_GET,
        .handler = handle_get_state,
        .description = "Get plug state"
    },
    {
        .uri = "/state",
        .method = ACTION_PUT,
        .handler = handle_put_state,
        .description = "Set plug state"
    }
};

const device_config_t DEVICE_CONFIG = {
    .device_type = "smart_plug",
    .endpoints = plug_endpoints,
    .num_endpoints = sizeof(plug_endpoints) / sizeof(plug_endpoints[0]),
    .init = plug_init,
    .deinit = NULL
};
