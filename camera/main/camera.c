#include <stdio.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"

#define WIFI_SSID "SamDach"
#define WIFI_PASS "hahahaha"

static const char *TAG = "camera";

// WiFi event handler
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying WiFi connection...");
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        char ip_str[16];
        ESP_LOGI(TAG, "Got IP: %s", esp_ip4addr_ntoa(&event->ip_info.ip, ip_str, sizeof(ip_str)));
    }
}

// Initialize WiFi
void init_wifi() {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
}

void start_camera_server() {
    // TODO: Implement the actual camera server functionality
    ESP_LOGI("Camera Server", "Camera server started (placeholder)");
}

void app_main() {
    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "Initializing WiFi");
    init_wifi();

    ESP_LOGI(TAG, "Connecting to WiFi...");
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Connected to WiFi: %s", ap_info.ssid);
            break;
        } else {
            ESP_LOGI(TAG, "Still trying to connect to WiFi...");
        }
    }

    ESP_LOGI(TAG, "Initializing Camera");
    camera_config_t config = {
        .pin_pwdn = -1, // Power down pin not used
        .pin_reset = -1, // Reset pin not used
        .pin_xclk = 0, // XCLK pin
        .pin_sscb_sda = 26, // SDA pin for SCCB
        .pin_sscb_scl = 27, // SCL pin for SCCB
        .pin_d7 = 35, // Data pin 7
        .pin_d6 = 34, // Data pin 6
        .pin_d5 = 39, // Data pin 5
        .pin_d4 = 36, // Data pin 4
        .pin_d3 = 21, // Data pin 3
        .pin_d2 = 19, // Data pin 2
        .pin_d1 = 18, // Data pin 1
        .pin_d0 = 5, // Data pin 0
        .pin_vsync = 25, // VSYNC pin
        .pin_href = 23, // HREF pin
        .pin_pclk = 22, // PCLK pin
        .xclk_freq_hz = 20000000,
        .ledc_timer = LEDC_TIMER_0,
        .ledc_channel = LEDC_CHANNEL_0,
        .pixel_format = PIXFORMAT_JPEG,
        .frame_size = FRAMESIZE_UXGA,
        .jpeg_quality = 12,
        .fb_count = 2
    };

    ESP_ERROR_CHECK(esp_camera_init(&config));

    ESP_LOGI(TAG, "Starting Camera Server");
    start_camera_server();
}
