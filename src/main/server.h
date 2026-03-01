#ifndef SERVER_H
#define SERVER_H

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"

#define WIFI_AP_SSID      "ESP_GUITAR_TUNER"
#define WIFI_AP_PASS      "12345678"
#define WIFI_AP_CHANNEL   1
#define WIFI_AP_MAX_CONN  4

/**
 * Initialize and start the WiFi access point
 */
void wifi_ap_start(void);

/**
 * Start the HTTP web server
 * Serves static files from SPIFFS and provides JSON API endpoint
 */
void web_server_start(void);

/**
 * Update the current note detection data
 * 
 * @param note Name of the detected note (e.g., "D3")
 * @param frequency Detected frequency in Hz
 * @param cents Deviation from the target note in cents
 */
void web_server_update_note(const char *note, float frequency, float cents);

#endif // SERVER_H
