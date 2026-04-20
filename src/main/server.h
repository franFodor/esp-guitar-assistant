#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define WIFI_AP_SSID      "ESP_GUITAR_TUNER"
#define WIFI_AP_PASS      "12345678"
#define WIFI_AP_CHANNEL   1
#define WIFI_AP_MAX_CONN  4

typedef enum {
    DETECTION_MODE_NOTE = 0,
    DETECTION_MODE_CHORD = 1
} detection_mode_t;

void wifi_ap_start(void);
void web_server_start(void);

detection_mode_t web_server_get_mode(void);

void web_server_update_note(const char *note, float frequency, float cents);
void web_server_update_chord(const char *chord, const char notes[][8], int note_count);

#endif // SERVER_H
