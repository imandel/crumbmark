#ifndef DEVICE_ACTIONS_H
#define DEVICE_ACTIONS_H

#include "esp_err.h"
#include "cJSON.h"

// Define action types
typedef enum {
    ACTION_GET,
    ACTION_PUT,
    ACTION_POST,
    ACTION_DELETE
} action_method_t;

// Structure to hold action response
typedef struct {
    char *response;
    esp_err_t status;
} action_response_t;

// Function pointer type for actions
typedef action_response_t (*action_handler_t)(const cJSON *request);

// Structure to define an endpoint
typedef struct {
    const char *uri;                // Endpoint path
    action_method_t method;         // HTTP method
    action_handler_t handler;       // Handler function
    const char *description;        // Endpoint description
} device_endpoint_t;

// Device configuration structure
typedef struct {
    const char *device_type;                    // Type of device (e.g., "plug", "sensor")
    const device_endpoint_t *endpoints;         // Array of endpoints
    int num_endpoints;                          // Number of endpoints
    esp_err_t (*init)(void);                   // Device initialization function
    void (*deinit)(void);                      // Device cleanup function
} device_config_t;

#endif // DEVICE_ACTIONS_H
