#include "server.h"
#include "chord.h"

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


static const char *TAG = "wifi_ap";


/* ---------- HTTP HANDLERS ---------- */
static esp_err_t test_handler(httpd_req_t *req) {
    httpd_resp_sendstr(req, "SERVER OK");
    return ESP_OK;
}

static esp_err_t file_get_handler(httpd_req_t *req) {
    char path[128];

    if (strcmp(req->uri, "/") == 0) {
        strcpy(path, "/spiffs/index.html");
    } else {
        snprintf(path, sizeof(path),
                 "/spiffs%.*s",
                 (int)(sizeof(path) - 8),
                 req->uri);
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    if (strstr(path, ".html")) httpd_resp_set_type(req, "text/html");
    else if (strstr(path, ".css")) httpd_resp_set_type(req, "text/css");
    else if (strstr(path, ".js")) httpd_resp_set_type(req, "application/javascript");

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

/* ---------- PUBLIC API ---------- */
detection_mode_t web_server_get_mode(void) {
    return current_mode;
}


void web_server_update_note(const char *note, float frequency, float cents) {
    if (!note_mutex) return;

    xSemaphoreTake(note_mutex, portMAX_DELAY);
    strncpy(current_note, note, sizeof(current_note) - 1);
    current_note[sizeof(current_note) - 1] = 0;
    current_freq = frequency;
    current_cents = cents;

    snprintf(cached_note_response, sizeof(cached_note_response),
             "{\"note\":\"%s\",\"frequency\":%.2f,\"cents\":%.1f}",
             current_note, current_freq, current_cents);

    xSemaphoreGive(note_mutex);
}

void web_server_update_chord(const char *chord, const char notes[][8], int note_count) {
    if (!chord_mutex) return;

    xSemaphoreTake(chord_mutex, portMAX_DELAY);
    strncpy(current_chord, chord, sizeof(current_chord) - 1);
    current_chord[sizeof(current_chord) - 1] = 0;

    current_note_count = note_count;
    for (int i = 0; i < note_count && i < MAX_CHORD_NOTES; i++) {
        strncpy(current_notes[i], notes[i], 7);
        current_notes[i][7] = '\0';
    }

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
}

void web_server_start(void) {
    note_mutex  = xSemaphoreCreateMutex();
    chord_mutex = xSemaphoreCreateMutex();

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

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

    httpd_register_uri_handler(server, &api_note);
    httpd_register_uri_handler(server, &api_chord);
    httpd_register_uri_handler(server, &api_mode);
    httpd_register_uri_handler(server, &test);
    httpd_register_uri_handler(server, &files);
}

void wifi_ap_start(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    esp_netif_init();
    esp_event_loop_create_default();

    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    wifi_config_t ap_cfg = {
        .ap = {
            .channel = WIFI_AP_CHANNEL,
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };

    strncpy((char *)ap_cfg.ap.ssid, WIFI_AP_SSID, sizeof(ap_cfg.ap.ssid));
    strncpy((char *)ap_cfg.ap.password, WIFI_AP_PASS, sizeof(ap_cfg.ap.password));
    ap_cfg.ap.ssid_len = strlen(WIFI_AP_SSID);

    if (strlen(WIFI_AP_PASS) == 0)
        ap_cfg.ap.authmode = WIFI_AUTH_OPEN;

    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    ESP_LOGI(TAG, "WiFi AP started");
}
