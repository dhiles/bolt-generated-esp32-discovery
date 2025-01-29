#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#define DISCOVERY_PORT 12345
#define DISCOVERY_MSG "DISCOVER"
#define RESPONSE_MSG "RESPONSE"
#define MAX_RESPONDERS 10

typedef enum {
    HUB,
    ROUTER,
    CAM,
    MOTION_SENSOR,
    TEMP_SENSOR
} DeviceType;

typedef struct {
    char ip[16];
    DeviceType type;
} Responder;

Responder responders[MAX_RESPONDERS];
int responder_count = 0;

static const char *TAG = "Discovery";

void discovery_task(void *pvParameters) {
    struct sockaddr_in dest_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(DISCOVERY_PORT);
    inet_pton(AF_INET, "255.255.255.255", &dest_addr.sin_addr);

    int err = sendto(sock, DISCOVERY_MSG, strlen(DISCOVERY_MSG), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
    }

    close(sock);
    vTaskDelete(NULL);
}

void response_listener_task(void *pvParameters) {
    struct sockaddr_in listen_addr;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
    }

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_port = htons(DISCOVERY_PORT);
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int err = bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
    }

    char rx_buffer[128];
    struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);

    while (1) {
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &addr_len);
        if (len < 0) {
            ESP_LOGE(TAG, "recvfrom failed: errno %d", errno);
            break;
        }

        rx_buffer[len] = 0; // Null-terminate received string

        if (strncmp(rx_buffer, RESPONSE_MSG, strlen(RESPONSE_MSG)) == 0) {
            char *ip = inet_ntoa(source_addr.sin_addr);
            char *type_str = rx_buffer + strlen(RESPONSE_MSG) + 1;
            DeviceType type = HUB; // Default type

            if (strcmp(type_str, "HUB") == 0) {
                type = HUB;
            } else if (strcmp(type_str, "ROUTER") == 0) {
                type = ROUTER;
            } else if (strcmp(type_str, "CAM") == 0) {
                type = CAM;
            } else if (strcmp(type_str, "MOTION_SENSOR") == 0) {
                type = MOTION_SENSOR;
            } else if (strcmp(type_str, "TEMP_SENSOR") == 0) {
                type = TEMP_SENSOR;
            }

            if (responder_count < MAX_RESPONDERS) {
                strcpy(responders[responder_count].ip, ip);
                responders[responder_count].type = type;
                responder_count++;
                ESP_LOGI(TAG, "Found device at IP: %s, Type: %d", ip, type);
            }
        }
    }

    close(sock);
    vTaskDelete(NULL);
}

void app_main(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    esp_netif_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "your_SSID",
            .password = "your_PASSWORD",
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    xTaskCreate(discovery_task, "discovery_task", 4096, NULL, 5, NULL);
    xTaskCreate(response_listener_task, "response_listener_task", 4096, NULL, 5, NULL);
}
