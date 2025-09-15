
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"

#define WIFI_SSID "ESP32-AP"
#define WIFI_PASS "esp32password"

static const char *TAG = "esp32-ap";

esp_err_t hello_get_handler(httpd_req_t *req) {
    const char* resp_str = "Hello, World!";
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

httpd_uri_t hello = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = hello_get_handler,
    .user_ctx = NULL
};

void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &hello);
    }
}

void wifi_init_softap(void) {
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config);
    esp_wifi_start();
    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
}

void app_main(void) {
    nvs_flash_init();
    wifi_init_softap();
    start_webserver();
    ESP_LOGI(TAG, "HTTP server started");
}
