#include "server.h"
#include "chord.h"
#include "wifi_manager.h"
#include <errno.h>

static httpd_handle_t server = NULL;

// Detection mode
static volatile detection_mode_t current_mode = DETECTION_MODE_NOTE;

// Note detection data
static char current_note[8] = "None";
static float current_freq = 0.0f;
static float current_cents = 0.0f;
static SemaphoreHandle_t note_mutex;
static char cached_note_response[128] = "{\"note\":\"None\",\"frequency\":0.00,\"cents\":0.0}";

// Chord detection data
static char current_chord[32] = "None";
static char current_notes[MAX_CHORD_NOTES][8] = {""};
static int current_note_count = 0;
static SemaphoreHandle_t chord_mutex;
static char cached_chord_response[256] = "{\"chord\":\"None\",\"notes\":[]}";

// SSE semaphores — given each time new note/chord data is ready
static SemaphoreHandle_t note_sse_sem  = NULL;
static SemaphoreHandle_t chord_sse_sem = NULL;


static const char *TAG = "wifi_ap";

/* ---------- SSE TASKS ---------- */

static void note_sse_task(void *arg) {
    httpd_req_t *req = (httpd_req_t *)arg;
    char buf[160];
    while (1) {
        if (xSemaphoreTake(note_sse_sem, pdMS_TO_TICKS(20000)) == pdTRUE) {
            xSemaphoreTake(note_mutex, portMAX_DELAY);
            int len = snprintf(buf, sizeof(buf), "data: %s\n\n", cached_note_response);
            xSemaphoreGive(note_mutex);
            if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                if (errno == EAGAIN) continue;  // WiFi busy (e.g. scan), skip event
                break;
            }
        } else {
            if (httpd_resp_send_chunk(req, ": ping\n\n", 8) != ESP_OK) {
                if (errno == EAGAIN) continue;
                break;
            }
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    httpd_req_async_handler_complete(req);
    vTaskDelete(NULL);
}

static void chord_sse_task(void *arg) {
    httpd_req_t *req = (httpd_req_t *)arg;
    char buf[288];
    while (1) {
        if (xSemaphoreTake(chord_sse_sem, pdMS_TO_TICKS(20000)) == pdTRUE) {
            xSemaphoreTake(chord_mutex, portMAX_DELAY);
            int len = snprintf(buf, sizeof(buf), "data: %s\n\n", cached_chord_response);
            xSemaphoreGive(chord_mutex);
            if (httpd_resp_send_chunk(req, buf, len) != ESP_OK) {
                if (errno == EAGAIN) continue;
                break;
            }
        } else {
            if (httpd_resp_send_chunk(req, ": ping\n\n", 8) != ESP_OK) {
                if (errno == EAGAIN) continue;
                break;
            }
        }
    }
    httpd_resp_send_chunk(req, NULL, 0);
    httpd_req_async_handler_complete(req);
    vTaskDelete(NULL);
}

static esp_err_t api_note_sse_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_req_t *async_req;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "async failed");
        return ESP_FAIL;
    }
    if (xTaskCreate(note_sse_task, "note_sse", 8192, async_req, 3, NULL) != pdPASS) {
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t api_chord_sse_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/event-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "keep-alive");
    httpd_req_t *async_req;
    if (httpd_req_async_handler_begin(req, &async_req) != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "async failed");
        return ESP_FAIL;
    }
    if (xTaskCreate(chord_sse_task, "chord_sse", 8192, async_req, 3, NULL) != pdPASS) {
        httpd_req_async_handler_complete(async_req);
        return ESP_FAIL;
    }
    return ESP_OK;
}


/* ---------- HTTP HANDLERS ---------- */
static esp_err_t test_handler(httpd_req_t *req) {
    httpd_resp_sendstr(req, "SERVER OK");
    return ESP_OK;
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    // Map the URI to a SPIFFS path; root URL serves index.html
    char path[128];
    if (strcmp(req->uri, "/") == 0) {
        strcpy(path, "/spiffs/index.html");
    } else {
        snprintf(path, sizeof(path), "/spiffs%.*s",
                 (int)(sizeof(path) - 8), req->uri);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_OK;
    }

    // Set Content-Type and caching based on file extension.
    // JS and CSS are versioned by content so they can be cached long-term;
    // HTML is not cached so navigation always loads the latest markup.
    if (strstr(path, ".html")) {
        httpd_resp_set_type(req, "text/html");
    } else if (strstr(path, ".css")) {
        httpd_resp_set_type(req, "text/css");
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    } else if (strstr(path, ".js")) {
        httpd_resp_set_type(req, "application/javascript");
        httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    }

    // Stream file contents in 1 KB chunks to avoid large stack buffers
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        httpd_resp_send_chunk(req, buf, n);
    }

    fclose(f);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t api_note_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, cached_note_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t api_chord_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, cached_chord_response, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t api_mode_handler(httpd_req_t *req) {
    char body[16] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing body");
        return ESP_FAIL;
    }
    body[len] = '\0';

    if (strncmp(body, "chord", 5) == 0) {
        current_mode = DETECTION_MODE_CHORD;
        ESP_LOGI(TAG, "Mode -> CHORD");
    } else {
        current_mode = DETECTION_MODE_NOTE;
        ESP_LOGI(TAG, "Mode -> NOTE");
    }

    httpd_resp_sendstr(req, "OK");
    return ESP_OK;
}

/* ---------- WiFi API HANDLERS ---------- */
static esp_err_t api_wifi_status_handler(httpd_req_t *req) {
    char buf[256];
    wifi_manager_status_json(buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t api_wifi_scan_handler(httpd_req_t *req) {
    char buf[1200];
    wifi_manager_scan_json(buf, sizeof(buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, buf);
    return ESP_OK;
}

static esp_err_t api_wifi_connect_handler(httpd_req_t *req) {
    char body[200] = {0};
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing body");
        return ESP_FAIL;
    }
    body[len] = '\0';

    // Simple extraction: find "ssid":"..." and "pass":"..."
    char *sp = strstr(body, "\"ssid\":\"");
    char *pp = strstr(body, "\"pass\":\"");
    if (!sp) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing ssid");
        return ESP_FAIL;
    }
    sp += 8;
    char ssid[33] = {0}, pass[65] = {0};
    for (int i = 0; i < 32 && sp[i] && sp[i] != '"'; i++) ssid[i] = sp[i];
    if (pp) {
        pp += 8;
        for (int i = 0; i < 64 && pp[i] && pp[i] != '"'; i++) pass[i] = pp[i];
    }

    wifi_manager_connect(ssid, pass);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"connecting\"}");
    return ESP_OK;
}

static esp_err_t api_wifi_disconnect_handler(httpd_req_t *req) {
    wifi_manager_disconnect();
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ap\"}");
    return ESP_OK;
}

/* ---------- PUBLIC API ---------- */
detection_mode_t web_server_get_mode(void) {
    return current_mode;
}


void web_server_update_note(const char *note, float frequency, float cents) {
    if (!note_mutex) return;

    // Update state and rebuild the cached JSON string under the mutex.
    // Pre-building the response string here (instead of in the HTTP handler)
    // keeps the handler lock-free and fast — it just sends the cached string.
    xSemaphoreTake(note_mutex, portMAX_DELAY);
    strncpy(current_note, note, sizeof(current_note) - 1);
    current_note[sizeof(current_note) - 1] = 0;
    current_freq = frequency;
    current_cents = cents;
    snprintf(cached_note_response, sizeof(cached_note_response),
             "{\"note\":\"%s\",\"frequency\":%.2f,\"cents\":%.1f}",
             current_note, current_freq, current_cents);
    xSemaphoreGive(note_mutex);
    if (note_sse_sem) xSemaphoreGive(note_sse_sem);
}

void web_server_update_chord(const char *chord, const char notes[][8], int note_count) {
    if (!chord_mutex) return;

    xSemaphoreTake(chord_mutex, portMAX_DELAY);

    // Copy chord name and individual notes into the cached state
    strncpy(current_chord, chord, sizeof(current_chord) - 1);
    current_chord[sizeof(current_chord) - 1] = 0;
    current_note_count = note_count;
    for (int i = 0; i < note_count && i < MAX_CHORD_NOTES; i++) {
        strncpy(current_notes[i], notes[i], 7);
        current_notes[i][7] = '\0';
    }

    // Build the notes JSON array, then embed it in the full response string
    char notes_json[128] = "[]";
    if (note_count > 0) {
        char* ptr = notes_json;
        ptr += sprintf(ptr, "[");
        for (int i = 0; i < note_count; i++) {
            if (i > 0) ptr += sprintf(ptr, ",");
            ptr += sprintf(ptr, "\"%s\"", current_notes[i]);
        }
        ptr += sprintf(ptr, "]");
    }
    snprintf(cached_chord_response, sizeof(cached_chord_response),
             "{\"chord\":\"%s\",\"notes\":%s}",
             current_chord, notes_json);
    xSemaphoreGive(chord_mutex);
    if (chord_sse_sem) xSemaphoreGive(chord_sse_sem);
}

void web_server_start(void) {
    note_mutex  = xSemaphoreCreateMutex();
    chord_mutex = xSemaphoreCreateMutex();
    note_sse_sem  = xSemaphoreCreateBinary();
    chord_sse_sem = xSemaphoreCreateBinary();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn   = httpd_uri_match_wildcard;
    config.max_uri_handlers = 14;

    httpd_start(&server, &config);

    httpd_uri_t files = {
        .uri      = "/*",
        .method   = HTTP_GET,
        .handler  = file_get_handler
    };

    httpd_uri_t api_note = {
        .uri      = "/api/note",
        .method   = HTTP_GET,
        .handler  = api_note_handler
    };

    httpd_uri_t api_chord = {
        .uri      = "/api/chord",
        .method   = HTTP_GET,
        .handler  = api_chord_handler
    };

    httpd_uri_t api_mode = {
        .uri      = "/api/mode",
        .method   = HTTP_POST,
        .handler  = api_mode_handler
    };

    httpd_uri_t test = {
        .uri      = "/test",
        .method   = HTTP_GET,
        .handler  = test_handler
    };

    httpd_uri_t api_wifi_status = {
        .uri     = "/api/wifi/status",
        .method  = HTTP_GET,
        .handler = api_wifi_status_handler
    };
    httpd_uri_t api_wifi_scan = {
        .uri     = "/api/wifi/scan",
        .method  = HTTP_POST,
        .handler = api_wifi_scan_handler
    };
    httpd_uri_t api_wifi_connect = {
        .uri     = "/api/wifi/connect",
        .method  = HTTP_POST,
        .handler = api_wifi_connect_handler
    };
    httpd_uri_t api_wifi_disconnect = {
        .uri     = "/api/wifi/disconnect",
        .method  = HTTP_POST,
        .handler = api_wifi_disconnect_handler
    };

    httpd_uri_t api_note_sse = {
        .uri     = "/api/note/events",
        .method  = HTTP_GET,
        .handler = api_note_sse_handler
    };

    httpd_uri_t api_chord_sse = {
        .uri     = "/api/chord/events",
        .method  = HTTP_GET,
        .handler = api_chord_sse_handler
    };

    httpd_register_uri_handler(server, &api_note_sse);
    httpd_register_uri_handler(server, &api_chord_sse);
    httpd_register_uri_handler(server, &api_note);
    httpd_register_uri_handler(server, &api_chord);
    httpd_register_uri_handler(server, &api_mode);
    httpd_register_uri_handler(server, &api_wifi_status);
    httpd_register_uri_handler(server, &api_wifi_scan);
    httpd_register_uri_handler(server, &api_wifi_connect);
    httpd_register_uri_handler(server, &api_wifi_disconnect);
    httpd_register_uri_handler(server, &test);
    httpd_register_uri_handler(server, &files);
}

