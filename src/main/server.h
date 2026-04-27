#ifndef SERVER_H
#define SERVER_H

/**
 * @file server.h
 * @brief HTTP server, Wi-Fi AP, and shared audio-detection state.
 *
 * Exposes a demand-driven JSON API consumed by the web front-end:
 *   - GET  /api/note   — wakes the audio processor, returns one note result.
 *   - GET  /api/chord  — wakes the audio processor, returns one chord result.
 *   - POST /api/mode   — switches between "note" and "chord" detection modes.
 *
 * Each API call blocks until the processor task completes one FFT frame and
 * signals result_ready_sem, ensuring every response contains fresh data.
 */

#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

/** SSID broadcast by the ESP32 soft access point. */
#define WIFI_AP_SSID      "ESP_GUITAR_TUNER"

/** WPA2-PSK passphrase.  Set to "" for an open (no-password) network. */
#define WIFI_AP_PASS      "12345678"

/** 802.11 channel used by the access point (1–13). */
#define WIFI_AP_CHANNEL   1

/** Maximum number of simultaneous Wi-Fi client connections. */
#define WIFI_AP_MAX_CONN  4

/**
 * @brief Active audio-detection mode.
 */
typedef enum {
    DETECTION_MODE_NOTE  = 0, /**< Single-note detection (tuner page). */
    DETECTION_MODE_CHORD = 1, /**< Polyphonic chord recognition. */
} detection_mode_t;

/**
 * @brief Initialise NVS, the TCP/IP stack, and start the Wi-Fi access point.
 *
 * Must be called before web_server_start().
 */
void wifi_ap_start(void);

/**
 * @brief Create HTTP server mutexes/semaphores and register all URI handlers.
 *
 * Call web_server_set_processor_task() after creating the audio processor
 * task so that API requests can wake it.
 */
void web_server_start(void);

/**
 * @brief Return the currently active detection mode.
 *
 * Thread-safe; may be called from any task context.
 *
 * @return @c DETECTION_MODE_NOTE or @c DETECTION_MODE_CHORD.
 */
detection_mode_t web_server_get_mode(void);

/**
 * @brief Update the cached note-detection result.
 *
 * Acquires the note mutex, updates internal state, and rebuilds the JSON
 * string returned by GET /api/note.  Thread-safe.
 *
 * @param note       Null-terminated note name, e.g. @c "A4" or @c "None".
 * @param frequency  Detected frequency in Hz; 0.0 indicates no detection.
 * @param cents      Deviation from the nearest reference pitch in cents.
 */
void web_server_update_note(const char *note, float frequency, float cents);

/**
 * @brief Update the cached chord-detection result.
 *
 * Acquires the chord mutex, updates internal state, and rebuilds the JSON
 * string returned by GET /api/chord.  Thread-safe.
 *
 * @param chord       Null-terminated chord name, e.g. @c "A major" or @c "None".
 * @param notes       2-D array of note-name strings (may be NULL when
 *                    @p note_count is 0).
 * @param note_count  Number of valid entries in @p notes (0–MAX_CHORD_NOTES).
 */
void web_server_update_chord(const char *chord, const char notes[][8], int note_count);

/**
 * @brief Register the audio processor task for demand-driven wakeup.
 *
 * Must be called once after xTaskCreate() returns the processor task handle,
 * before the first API request is served.
 *
 * @param handle  FreeRTOS handle of the audio processor task.
 */
void web_server_set_processor_task(TaskHandle_t handle);

/**
 * @brief Signal that the current processing frame is complete.
 *
 * Called by the audio processor task after writing the result with
 * web_server_update_note() or web_server_update_chord().  Releases the
 * binary semaphore the waiting API handler is blocked on.
 */
void web_server_signal_result_ready(void);

#endif // SERVER_H
