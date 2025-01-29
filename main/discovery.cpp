#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_now.h"
#include "discovery.h"

typedef enum {
    DISCOVERY_REQUEST,
    DISCOVERY_RESPONSE
} MessageType;

typedef struct {
    MessageType type;
    char message[32];
} DiscoveryMsg;

void discovery_task(void *pvParameters) {
    uint8_t *mac_address = (uint8_t *)pvParameters;
    DiscoveryMsg discovery_msg = {DISCOVERY_REQUEST, "DISCOVER"};

    esp_err_t result = esp_now_send(mac_address, (uint8_t *)&discovery_msg, sizeof(discovery_msg));
    if (result == ESP_OK) {
        ESP_LOGI("Discovery", "Sent with success to MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    } else {
        ESP_LOGE("Discovery", "Error sending the data to MAC: %02x:%02x:%02x:%02x:%02x:%02x",
                 mac_address[0], mac_address[1], mac_address[2], mac_address[3], mac_address[4], mac_address[5]);
    }

    vTaskDelete(NULL);
}
