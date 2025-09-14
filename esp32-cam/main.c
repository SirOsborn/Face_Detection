// main.c - ESP32-CAM entry point
#include "camera.h"
#include "wifi.h"
#include "http_client.h"

void app_main() {
    camera_init();
    wifi_init();
    while (1) {
        image_t img = camera_capture();
        http_send_image(img);
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Capture every second
    }
}
