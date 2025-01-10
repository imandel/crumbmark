#include "device_actions.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "hlw8012.h"
#include <string.h>

#define PLUG_PIN GPIO_NUM_5
#define HLW8012_SEL_PIN GPIO_NUM_12
#define HLW8012_CF_PIN  GPIO_NUM_13
#define HLW8012_CF1_PIN GPIO_NUM_14
static const char *TAG = "smart_plug";
static int plug_state = 0;

static action_response_t handle_get_state(const cJSON *request) {
    action_response_t response = {0};
    cJSON *root = cJSON_CreateObject();
    hlw8012_readings_t readings = {0};  // Initialize to zeros
    
    // Add basic state
    cJSON_AddNumberToObject(root, "state", plug_state);
    
    // Always add power readings, even if zero
    esp_err_t ret = hlw8012_get_readings(&readings);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "HLW8012 read failed: %d, returning zeros", ret);
        // readings struct already initialized to zeros
    }
    
    // Always include these fields
    cJSON_AddNumberToObject(root, "voltage", readings.voltage);
    cJSON_AddNumberToObject(root, "current", readings.current);
    cJSON_AddNumberToObject(root, "power", readings.power);
    cJSON_AddNumberToObject(root, "energy", readings.energy);
    
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
    // Configure relay pin
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PLUG_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    
    // Initialize HLW8012
    hlw8012_config_t hlw_config = {
        .sel_pin = HLW8012_SEL_PIN,
        .cf_pin = HLW8012_CF_PIN,
        .cf1_pin = HLW8012_CF1_PIN,
        .current_multiplier = 0.001,  // Adjust based on calibration
        .voltage_multiplier = 0.4,    // Adjust based on calibration
        .power_multiplier = 0.2,      // Adjust based on calibration
    };
    return hlw8012_init(&hlw_config);
}

static action_response_t handle_get_power(const cJSON *request) {
    action_response_t response = {0};
    hlw8012_readings_t readings;
    
    if (hlw8012_get_readings(&readings) == ESP_OK) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddNumberToObject(root, "voltage", readings.voltage);
        cJSON_AddNumberToObject(root, "current", readings.current);
        cJSON_AddNumberToObject(root, "power", readings.power);
        cJSON_AddNumberToObject(root, "energy", readings.energy);
        response.response = cJSON_Print(root);
        response.status = ESP_OK;
        cJSON_Delete(root);
    } else {
        response.status = ESP_FAIL;
        response.response = strdup("Failed to read power measurements");
    }
    return response;
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
    },
    {
        .uri = "/power",
        .method = ACTION_GET,
        .handler = handle_get_power,
        .description = "Get power measurements"
    }
};

const device_config_t DEVICE_CONFIG = {
    .device_type = "smart_plug",
    .endpoints = plug_endpoints,
    .num_endpoints = sizeof(plug_endpoints) / sizeof(plug_endpoints[0]),
    .init = plug_init,
    .deinit = NULL
};
