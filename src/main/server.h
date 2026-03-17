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

/**
 * Update the current chord detection data
 *
 * @param chord Name of the detected chord (e.g., "C major")
 * @param notes Array of individual note names (e.g., ["C", "E", "G"])
 * @param note_count Number of notes in the chord
 */
void web_server_update_chord(const char *chord, const char notes[][8], int note_count);

#endif // SERVER_H
