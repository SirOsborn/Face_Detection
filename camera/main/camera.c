#include <stdio.h>
#include <string.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "driver/gpio.h"

#define WIFI_SSID "//H@ack.onion/terminal01"
#define WIFI_PASS "Wifi Kaeng Huey"

static const char *TAG = "ESP32S_Camera";

// Camera pin definitions for ESP32-S with OV3660 on ESP32-CAM-MB
#define CAM_PIN_PWDN    32
#define CAM_PIN_RESET   -1  // Not connected
#define CAM_PIN_XCLK    0
#define CAM_PIN_SIOD    26  // SDA
#define CAM_PIN_SIOC    27  // SCL
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      21
#define CAM_PIN_D2      19
#define CAM_PIN_D1      18
#define CAM_PIN_D0      5
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

// Flash LED pin (if available)
#define FLASH_LED_PIN   4

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

// Camera initialization function
esp_err_t init_camera() {
    camera_config_t config = {
        .pin_pwdn       = CAM_PIN_PWDN,
        .pin_reset      = CAM_PIN_RESET,
        .pin_xclk       = CAM_PIN_XCLK,
        .pin_sscb_sda   = CAM_PIN_SIOD,
        .pin_sscb_scl   = CAM_PIN_SIOC,
        .pin_d7         = CAM_PIN_D7,
        .pin_d6         = CAM_PIN_D6,
        .pin_d5         = CAM_PIN_D5,
        .pin_d4         = CAM_PIN_D4,
        .pin_d3         = CAM_PIN_D3,
        .pin_d2         = CAM_PIN_D2,
        .pin_d1         = CAM_PIN_D1,
        .pin_d0         = CAM_PIN_D0,
        .pin_vsync      = CAM_PIN_VSYNC,
        .pin_href       = CAM_PIN_HREF,
        .pin_pclk       = CAM_PIN_PCLK,
        .xclk_freq_hz   = 20000000,         // Reduce clock to prevent timing issues
        .ledc_timer     = LEDC_TIMER_0,
        .ledc_channel   = LEDC_CHANNEL_0,
        .pixel_format   = PIXFORMAT_JPEG,
        .frame_size     = FRAMESIZE_VGA,    // 640x480 - LARGER VIEW!
        .jpeg_quality   = 10,               // Higher quality to see if corruption persists
        .fb_count       = 1,                // Single buffer to reduce memory issues
        .fb_location    = CAMERA_FB_IN_DRAM,// Force frame buffer in DRAM
        .grab_mode      = CAMERA_GRAB_LATEST // Always grab latest frame
    };

    // Power down the camera to reset it
    if (CAM_PIN_PWDN != -1) {
        gpio_set_direction(CAM_PIN_PWDN, GPIO_MODE_OUTPUT);
        gpio_set_level(CAM_PIN_PWDN, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(CAM_PIN_PWDN, 0);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Initialize the camera
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x (%s)", err, esp_err_to_name(err));
        return err;
    }

    // Get sensor and configure it
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        // RESET TO BASIC REAL CAMERA SETTINGS - NO TEST PATTERNS
        s->set_brightness(s, 0);     // -2 to 2 (0 = normal)
        s->set_contrast(s, 0);       // -2 to 2 (0 = normal)
        s->set_saturation(s, 0);     // -2 to 2 (0 = normal)
        s->set_special_effect(s, 0); // 0 to 6 (0 - No Effect)
        s->set_whitebal(s, 1);       // 0 = disable , 1 = enable (auto white balance)
        s->set_awb_gain(s, 1);       // 0 = disable , 1 = enable (auto white balance gain)
        s->set_wb_mode(s, 0);        // 0 to 4 (0 = auto)
        s->set_exposure_ctrl(s, 1);  // 0 = disable , 1 = enable (auto exposure)
        s->set_aec2(s, 0);           // 0 = disable , 1 = enable (DISABLE advanced exposure)
        s->set_ae_level(s, 0);       // -2 to 2 (0 = normal)
        s->set_aec_value(s, 300);    // 0 to 1200 (normal exposure)
        s->set_gain_ctrl(s, 1);      // 0 = disable , 1 = enable (auto gain)
        s->set_agc_gain(s, 5);       // 0 to 30 (moderate gain)
        s->set_gainceiling(s, (gainceiling_t)2);  // 0 to 6 (normal ceiling)
        s->set_bpc(s, 0);            // 0 = disable , 1 = enable (DISABLE - might cause artifacts)
        s->set_wpc(s, 1);            // 0 = disable , 1 = enable (white pixel correction)
        s->set_raw_gma(s, 1);        // 0 = disable , 1 = enable (gamma correction)
        s->set_lenc(s, 1);           // 0 = disable , 1 = enable (lens correction)
        s->set_hmirror(s, 0);        // 0 = disable , 1 = enable (horizontal mirror)
        s->set_vflip(s, 0);          // 0 = disable , 1 = enable (vertical flip)
        s->set_dcw(s, 1);            // 0 = disable , 1 = enable (downsize enable)
        s->set_colorbar(s, 0);       // 0 = disable , 1 = enable (ENSURE TEST PATTERN OFF!)
        
        ESP_LOGI(TAG, "OV3660 configured for REAL camera streaming - VGA 640x480 NORMAL MODE");
    }

    ESP_LOGI(TAG, "Camera initialized successfully");
    return ESP_OK;
}

// HTTP handler for capturing a single image
static esp_err_t capture_handler(httpd_req_t *req) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Camera capture failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    esp_err_t res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    esp_camera_fb_return(fb);
    return res;
}

// HTTP handler for camera stream
static esp_err_t stream_handler(httpd_req_t *req) {
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t *_jpg_buf = NULL;
    char part_buf[128];

    static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=123456789000000000000987654321";
    static const char* _STREAM_BOUNDARY = "\r\n--123456789000000000000987654321\r\n";
    static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\nX-Timestamp: %d\r\n\r\n";

    ESP_LOGI(TAG, "Stream handler started");

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set response type");
        return res;
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "X-Framerate", "10");

    // Send initial boundary
    res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send initial boundary");
        return res;
    }

    while (true) {
        fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE(TAG, "Camera capture failed in stream");
            vTaskDelay(pdMS_TO_TICKS(33));
            continue;
        }

        // Validate frame size (accept smaller frames while fixing exposure)
        if (fb->len < 1500) {
            ESP_LOGW(TAG, "Frame too small: %zu bytes", fb->len);
            esp_camera_fb_return(fb);
            vTaskDelay(pdMS_TO_TICKS(33));
            continue;
        }

        if (fb->len > 0 && fb->format == PIXFORMAT_JPEG) {
            _jpg_buf_len = fb->len;
            _jpg_buf = fb->buf;

            // Send part header
            size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len, (int)esp_log_timestamp());
            res = httpd_resp_send_chunk(req, part_buf, hlen);
            
            if (res == ESP_OK) {
                // Send image data
                res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
            }
            
            if (res == ESP_OK) {
                // Send boundary
                res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
            }
        } else {
            ESP_LOGE(TAG, "Invalid frame: len=%zu, format=%d", fb->len, fb->format);
        }

        esp_camera_fb_return(fb);
        fb = NULL;

        if (res != ESP_OK) {
            ESP_LOGI(TAG, "Stream connection closed by client");
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(66)); // 66ms delay for ~15 FPS (smooth streaming)
    }

    ESP_LOGI(TAG, "Stream handler ended");
    return res;
}

// HTTP handler for the root page
static esp_err_t index_handler(httpd_req_t *req) {
    const char* resp_str = 
        "<!DOCTYPE html>\n"
        "<html>\n"
        "<head>\n"
        "    <title>ESP32-S Camera OV3660</title>\n"
        "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
        "    <style>\n"
        "        body { font-family: Arial, sans-serif; text-align: center; margin: 10px; background: #f0f0f0; }\n"
        "        .container { max-width: 640px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; }\n"
        "        .stream { max-width: 100%; height: auto; border: 2px solid #333; border-radius: 5px; }\n"
        "        .controls { margin: 15px 0; }\n"
        "        button { padding: 8px 16px; margin: 3px; font-size: 14px; border: none; border-radius: 5px; cursor: pointer; }\n"
        "        .capture-btn { background: #4CAF50; color: white; }\n"
        "        .refresh-btn { background: #2196F3; color: white; }\n"
        "        .status { margin: 10px 0; padding: 10px; border-radius: 5px; }\n"
        "        .connected { background: #d4edda; color: #155724; }\n"
        "        .error { background: #f8d7da; color: #721c24; }\n"
        "        .info { font-size: 12px; color: #666; margin-top: 15px; }\n"
        "    </style>\n"
        "</head>\n"
        "<body>\n"
        "    <div class=\"container\">\n"
        "        <h1>ESP32-S Camera</h1>\n"
        "        <h3>OV3660 - 3MP Sensor</h3>\n"
        "        <img id=\"stream\" class=\"stream\" src=\"/stream\" />\n"
        "        <div class=\"controls\">\n"
        "            <button class=\"capture-btn\" onclick=\"capture()\">📷 Capture Photo</button>\n"
        "            <button class=\"refresh-btn\" onclick=\"location.reload()\">🔄 Refresh Stream</button>\n"
        "        </div>\n"
        "        <div id=\"status\" class=\"status connected\">📡 Connected - Streaming at ~20 FPS</div>\n"
        "        <div class=\"info\">\n"
        "            <p><strong>Resolution:</strong> 640x480 (VGA) | <strong>Quality:</strong> Optimized for streaming</p>\n"
        "            <p><strong>Performance:</strong> Double buffered | <strong>Memory:</strong> DRAM optimized</p>\n"
        "        </div>\n"
        "    </div>\n"
        "    <script>\n"
        "        function capture() {\n"
        "            window.open('/capture', '_blank');\n"
        "        }\n"
        "        \n"
        "        let errorCount = 0;\n"
        "        \n"
        "        document.getElementById('stream').onerror = function() {\n"
        "            errorCount++;\n"
        "            document.getElementById('status').className = 'status error';\n"
        "            document.getElementById('status').innerHTML = '❌ Stream Error - Check connection';\n"
        "            \n"
        "            // Auto-retry after 3 seconds\n"
        "            if (errorCount < 5) {\n"
        "                setTimeout(function() {\n"
        "                    document.getElementById('stream').src = '/stream?' + new Date().getTime();\n"
        "                }, 3000);\n"
        "            }\n"
        "        };\n"
        "        \n"
        "        document.getElementById('stream').onload = function() {\n"
        "            errorCount = 0;\n"
        "            document.getElementById('status').className = 'status connected';\n"
        "            document.getElementById('status').innerHTML = '📡 Connected - Streaming at ~10 FPS';\n"
        "        };\n"
        "        \n"
        "        // Add loading indicator\n"
        "        window.addEventListener('load', function() {\n"
        "            document.getElementById('status').innerHTML = '⏳ Loading stream...';\n"
        "        });\n"
        "    </script>\n"
        "</body>\n"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);
}

// Test handler for debugging
static esp_err_t test_handler(httpd_req_t *req) {
    const char* resp = "Camera server is working! Stream should be at /stream";
    httpd_resp_set_type(req, "text/plain");
    return httpd_resp_send(req, resp, strlen(resp));
}

// Start HTTP server
void start_camera_server() {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 3;        // Limit connections for performance
    config.task_priority = 5;           // Lower priority than camera task
    config.stack_size = 4096;           // Smaller stack to save memory
    config.core_id = 1;                 // Run on core 1 (camera on core 0)
    config.max_uri_handlers = 4;        // Limit handlers
    config.max_resp_headers = 8;        // Reduce headers
    config.backlog_conn = 2;            // Smaller backlog
    config.lru_purge_enable = true;     // Enable connection cleanup

    // Start the httpd server
    ESP_LOGI(TAG, "Starting optimized server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        httpd_uri_t index_uri = {
            .uri       = "/",
            .method    = HTTP_GET,
            .handler   = index_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &index_uri);

        httpd_uri_t capture_uri = {
            .uri       = "/capture",
            .method    = HTTP_GET,
            .handler   = capture_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &capture_uri);

        // Add test handler for debugging
        httpd_uri_t test_uri = {
            .uri       = "/test",
            .method    = HTTP_GET,
            .handler   = test_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &test_uri);

        httpd_uri_t stream_uri = {
            .uri       = "/stream",
            .method    = HTTP_GET,
            .handler   = stream_handler,
            .user_ctx  = NULL
        };
        httpd_register_uri_handler(server, &stream_uri);

        ESP_LOGI(TAG, "Camera server started successfully");
    } else {
        ESP_LOGE(TAG, "Error starting server!");
    }
}

void app_main() {
    ESP_LOGI(TAG, "ESP32-S Camera with OV3660 (3MP) starting up...");
    
    // Initialize NVS
    ESP_LOGI(TAG, "Initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGI(TAG, "Erasing NVS flash and reinitializing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize WiFi
    ESP_LOGI(TAG, "Initializing WiFi");
    init_wifi();

    // Wait for WiFi connection
    ESP_LOGI(TAG, "Connecting to WiFi: %s", WIFI_SSID);
    int wifi_retry_count = 0;
    while (wifi_retry_count < 30) {  // Wait up to 30 seconds
        vTaskDelay(pdMS_TO_TICKS(1000));
        wifi_ap_record_t ap_info;
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            ESP_LOGI(TAG, "Successfully connected to WiFi: %s", ap_info.ssid);
            ESP_LOGI(TAG, "RSSI: %d", ap_info.rssi);
            break;
        } else {
            wifi_retry_count++;
            ESP_LOGI(TAG, "Still connecting to WiFi... (%d/30)", wifi_retry_count);
        }
    }

    if (wifi_retry_count >= 30) {
        ESP_LOGE(TAG, "Failed to connect to WiFi after 30 seconds");
        ESP_LOGI(TAG, "Continuing with camera initialization anyway...");
    }

    // Get and display IP address
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&ip_info.ip));
        ESP_LOGI(TAG, "Netmask: " IPSTR, IP2STR(&ip_info.netmask));
        ESP_LOGI(TAG, "Gateway: " IPSTR, IP2STR(&ip_info.gw));
    }

    // Initialize camera
    ESP_LOGI(TAG, "Initializing ESP32-S Camera with OV3660 (3MP)...");
    esp_err_t cam_err = init_camera();
    if (cam_err != ESP_OK) {
        ESP_LOGE(TAG, "Camera initialization failed with error 0x%x (%s)", cam_err, esp_err_to_name(cam_err));
        ESP_LOGE(TAG, "Check camera wiring and pin connections");
        ESP_LOGE(TAG, "Verify OV3660 is properly connected to ESP32-S");
        
        // Print pin configuration for debugging
        ESP_LOGI(TAG, "Camera pin configuration:");
        ESP_LOGI(TAG, "PWDN: %d, RESET: %d, XCLK: %d", CAM_PIN_PWDN, CAM_PIN_RESET, CAM_PIN_XCLK);
        ESP_LOGI(TAG, "SIOD: %d, SIOC: %d", CAM_PIN_SIOD, CAM_PIN_SIOC);
        ESP_LOGI(TAG, "Data pins - D7:%d D6:%d D5:%d D4:%d D3:%d D2:%d D1:%d D0:%d", 
                 CAM_PIN_D7, CAM_PIN_D6, CAM_PIN_D5, CAM_PIN_D4, 
                 CAM_PIN_D3, CAM_PIN_D2, CAM_PIN_D1, CAM_PIN_D0);
        ESP_LOGI(TAG, "VSYNC: %d, HREF: %d, PCLK: %d", CAM_PIN_VSYNC, CAM_PIN_HREF, CAM_PIN_PCLK);
        
        // Continue without camera for debugging
        ESP_LOGI(TAG, "Continuing without camera for WiFi server testing...");
    } else {
        ESP_LOGI(TAG, "Camera initialized successfully!");
        
        // Test camera capture
        ESP_LOGI(TAG, "Testing camera capture...");
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            ESP_LOGI(TAG, "Test capture successful - Image size: %zu bytes, Resolution: %dx%d", 
                     fb->len, fb->width, fb->height);
            esp_camera_fb_return(fb);
        } else {
            ESP_LOGE(TAG, "Test capture failed");
        }
    }

    // Start HTTP server
    ESP_LOGI(TAG, "Starting Camera HTTP Server...");
    start_camera_server();
    
    // Wait a moment for server to fully start
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // Get fresh IP info for display
    esp_netif_ip_info_t current_ip;
    esp_netif_t *current_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (current_netif && esp_netif_get_ip_info(current_netif, &current_ip) == ESP_OK) {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Camera server is running!");
        ESP_LOGI(TAG, "Open your web browser and go to:");
        ESP_LOGI(TAG, "http://" IPSTR, IP2STR(&current_ip.ip));
        ESP_LOGI(TAG, "Endpoints:");
        ESP_LOGI(TAG, "  Main page: http://" IPSTR "/", IP2STR(&current_ip.ip));
        ESP_LOGI(TAG, "  Live stream: http://" IPSTR "/stream", IP2STR(&current_ip.ip));
        ESP_LOGI(TAG, "  Capture photo: http://" IPSTR "/capture", IP2STR(&current_ip.ip));
        ESP_LOGI(TAG, "========================================");
    } else {
        ESP_LOGI(TAG, "========================================");
        ESP_LOGI(TAG, "Camera server is running!");
        ESP_LOGI(TAG, "Open your web browser and go to:");
        ESP_LOGI(TAG, "http://192.168.10.125 (check your IP address)");
        ESP_LOGI(TAG, "========================================");
    }

    // Main loop - keep the application running
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second delay
        ESP_LOGI(TAG, "Camera server running... Free heap: %d bytes", esp_get_free_heap_size());
    }
}
