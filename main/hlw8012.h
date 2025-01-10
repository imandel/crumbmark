#ifndef HLW8012_H
#define HLW8012_H

#include "esp_err.h"
#include "driver/gpio.h"

typedef struct {
    gpio_num_t sel_pin;
    gpio_num_t cf_pin;
    gpio_num_t cf1_pin;
    float current_multiplier;
    float voltage_multiplier;
    float power_multiplier;
} hlw8012_config_t;

typedef struct {
    float voltage;      // V
    float current;      // A
    float power;        // W
    float energy;       // kWh
} hlw8012_readings_t;

esp_err_t hlw8012_init(const hlw8012_config_t *config);
esp_err_t hlw8012_get_readings(hlw8012_readings_t *readings);

#endif // HLW8012_H
