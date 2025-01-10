#include "hlw8012.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static hlw8012_config_t sensor_config;
static hlw8012_readings_t current_readings = {0};
static int64_t last_cf_pulse = 0;
static int64_t last_cf1_pulse = 0;

static void IRAM_ATTR cf_isr_handler(void* arg) {
    UBaseType_t saved_interrupt_status = taskENTER_CRITICAL_FROM_ISR();
    last_cf_pulse = esp_timer_get_time();
    taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_status);
}

static void IRAM_ATTR cf1_isr_handler(void* arg) {
    UBaseType_t saved_interrupt_status = taskENTER_CRITICAL_FROM_ISR();
    last_cf1_pulse = esp_timer_get_time();
    taskEXIT_CRITICAL_FROM_ISR(saved_interrupt_status);
}

esp_err_t hlw8012_init(const hlw8012_config_t *config) {
    sensor_config = *config;
    
    // Install GPIO ISR service
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    
    // Configure GPIO pins
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    
    // Configure CF pin
    io_conf.pin_bit_mask = (1ULL << config->cf_pin);
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(config->cf_pin, cf_isr_handler, NULL));
    
    // Configure CF1 pin
    io_conf.pin_bit_mask = (1ULL << config->cf1_pin);
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    ESP_ERROR_CHECK(gpio_isr_handler_add(config->cf1_pin, cf1_isr_handler, NULL));
    
    // Configure SEL pin as output
    gpio_config_t sel_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << config->sel_pin),
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&sel_conf));
    
    return ESP_OK;
}

esp_err_t hlw8012_get_readings(hlw8012_readings_t *readings) {
    if (!readings) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Toggle SEL pin to read different values
    gpio_set_level(sensor_config.sel_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));  // Wait for signal to stabilize
    
    // Calculate current
    int64_t current_period = esp_timer_get_time() - last_cf1_pulse;
    if (current_period > 0) {
        current_readings.current = sensor_config.current_multiplier / current_period;
    }
    
    gpio_set_level(sensor_config.sel_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Calculate voltage
    int64_t voltage_period = esp_timer_get_time() - last_cf1_pulse;
    if (voltage_period > 0) {
        current_readings.voltage = sensor_config.voltage_multiplier / voltage_period;
    }
    
    // Calculate power
    int64_t power_period = esp_timer_get_time() - last_cf_pulse;
    if (power_period > 0) {
        current_readings.power = sensor_config.power_multiplier / power_period;
    }
    
    // Update energy (kWh) based on power
    static int64_t last_energy_update = 0;
    int64_t now = esp_timer_get_time();
    if (last_energy_update > 0) {
        float hours = (now - last_energy_update) / (3600.0 * 1000000.0);
        current_readings.energy += (current_readings.power * hours) / 1000.0;
    }
    last_energy_update = now;
    
    *readings = current_readings;
    return ESP_OK;
}
