#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_now.h"
#include "esp_mac.h"
#include "discovery.h"

#define MAX_RESPONDERS 10
#define MY_ESPNOW_PMK "pmk12345678901234567890"

typedef enum {
    DISCOVERY_REQUEST,
    DISCOVERY_RESPONSE
} MessageType;

typedef enum {
    HUB,
    ROUTER,
    CAM,
    MOTION_SENSOR,
    TEMP_SENSOR
} DeviceType;

typedef struct {
    MessageType type;
    char message[32];
} DiscoveryMsg;

typedef struct {
    MessageType type;
    DeviceType device_type;
} ResponseMsg;

typedef struct {
    uint8_t mac_address[6];
    DeviceType type;
} Responder;

Responder responders[MAX_RESPONDERS];
int responder_count = 0;

static const char *TAG = "Discovery";

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "Last Packet Sent to: %s", macStr);
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Send Success");
    } else {
        ESP_LOGE(TAG, "Send Fail");
    }
}

// Callback when data is received
void OnDataRecv(const uint8_t *mac_addr, const uint8_t *incomingData, int len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
             mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
    ESP_LOGI(TAG, "Packet received from: %s", macStr);

    ResponseMsg *response = (ResponseMsg *)incomingData;
    if (response->type == DISCOVERY_RESPONSE) {
        DeviceType type = response->device_type;

        if (responder_count < MAX_RESPONDERS) {
            memcpy(responders[responder_count].mac_address, mac_addr, 6);
            responders[responder_count].type = type;
            responder_count++;
            ESP_LOGI(TAG, "Found device at MAC: %s, Type: %d", macStr, type);
        }
    }
}

static void init_espnow_master(void) {
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Now set the channel, as Wi-Fi is running
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    // Set maximum transmit power for better range
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(78));

    // Enable Long Range protocol
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(OnDataSent));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(OnDataRecv));
    ESP_ERROR_CHECK(esp_now_set_pmk((const uint8_t *)MY_ESPNOW_PMK));
}

void app_main(void) {
    init_espnow_master();

    // List of known MAC addresses to send discovery messages to
    uint8_t known_mac_addresses[][6] = {
        {0x24, 0x6F, 0x28, 0XX, 0XX, 0XX}, // Replace with actual MAC addresses
        {0x24, 0x6F, 0x28, 0XX, 0XX, 0XX},
        // Add more MAC addresses as needed
    };

    int num_known_mac_addresses = sizeof(known_mac_addresses) / sizeof(known_mac_addresses[0]);

    for (int i = 0; i < num_known_mac_addresses; i++) {
        xTaskCreate(discovery_task, "discovery_task", 4096, known_mac_addresses[i], 5, NULL);
    }
}
