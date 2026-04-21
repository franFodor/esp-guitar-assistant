#include "note.h"
#include "server.h"

static note_t chromatic_scale[] = {
    {"E2", 82.41}, {"F2", 87.31}, {"F#2", 92.50}, {"G2", 98.00}, {"G#2", 103.83},
    {"A2", 110.00}, {"A#2", 116.54}, {"B2", 123.47}, {"C3", 130.81}, {"C#3", 138.59},
    {"D3", 146.83}, {"D#3", 155.56}, {"E3", 164.81}, {"F3", 174.61}, {"F#3", 185.00},
    {"G3", 196.00}, {"G#3", 207.65}, {"A3", 220.00}, {"A#3", 233.08}, {"B3", 246.94},
    {"C4", 261.63}, {"C#4", 277.18}, {"D4", 293.66}, {"D#4", 311.13}, {"E4", 329.63},
    {"F4", 349.23}, {"F#4", 369.99}, {"G4", 392.00}, {"G#4", 415.30}, {"A4", 440.00},
    {"A#4", 466.16}, {"B4", 493.88}
};

#define NUM_NOTES (sizeof(chromatic_scale) / sizeof(chromatic_scale[0]))

// Persistence state: a note must be detected this many consecutive frames before publishing
#define NOTE_STABILITY_FRAMES 3

static float last_freq = 0.0f;
static int   no_detection_count = 0;
static char  pending_note[8] = "";
static int   pending_count = 0;

static float quadratic_interpolation(float* magnitudes, int peak_bin) {
    if (peak_bin <= 0 || peak_bin >= (FFT_SIZE/2 - 1))
        return (float)peak_bin * SAMPLE_RATE / FFT_SIZE;

    float gamma = magnitudes[peak_bin - 1];
    float beta  = magnitudes[peak_bin];
    float alpha = magnitudes[peak_bin + 1];

    float denom = (gamma - 2.0f * beta + alpha);
    if (fabsf(denom) < 1e-12f)
        return (float)peak_bin * SAMPLE_RATE / FFT_SIZE;

    float p = (alpha - gamma) / (2.0f * denom);
    return ((float)peak_bin + p) * SAMPLE_RATE / FFT_SIZE;
}

static float harmonic_product_spectrum(float* magnitudes, int half_size, float* hps) {
    float freq_res = (float)SAMPLE_RATE / FFT_SIZE;
    const int R = 4;

    for (int i = 0; i < half_size; i++) hps[i] = 1.0f;

    for (int r = 1; r <= R; r++) {
        int limit = half_size / r;
        for (int i = 0; i < limit; i++) hps[i] *= (magnitudes[i * r] + 1e-12f);
        for (int i = limit; i < half_size; i++) hps[i] = 0.0f;
    }

    int min_bin = (int)floorf(70.0f / freq_res);
    int max_bin = (int)ceilf(600.0f / freq_res);
    if (min_bin < 1) min_bin = 1;
    if (max_bin >= half_size) max_bin = half_size - 1;

    int best_bin = min_bin;
    float best_val = hps[min_bin];
    for (int i = min_bin + 1; i <= max_bin; i++) {
        if (hps[i] > best_val) { best_val = hps[i]; best_bin = i; }
    }

    // Octave correction: if the lower octave has >= 25% of the peak's HPS energy,
    // the guitar fundamental is likely there — prefer it over the harmonic.
    int lower_bin = best_bin / 2;
    if (lower_bin >= min_bin && hps[lower_bin] >= 0.25f * best_val)
        best_bin = lower_bin;

    return quadratic_interpolation(hps, best_bin);
}

const char* note_find_closest(float frequency, float* cents_offset) {
    const char* note = "Unknown";
    float min_diff = 1e9f;
    for (int i = 0; i < NUM_NOTES; i++) {
        float diff = fabsf(frequency - chromatic_scale[i].frequency);
        if (diff < min_diff) {
            min_diff = diff;
            note = chromatic_scale[i].name;
            *cents_offset = 1200.0f * log2f(frequency / chromatic_scale[i].frequency);
        }
    }
    return note;
}

void note_frequency_analysis(float* magnitudes, float* hps) {
    for (int i = 1; i < FFT_SIZE/2; i++)
        magnitudes[i] = powf(magnitudes[i] + 1e-20f, 0.65f);

    float freq = harmonic_product_spectrum(magnitudes, FFT_SIZE/2, hps);

    if (freq > 0.0f) {
        last_freq = freq;
        no_detection_count = 0;

        float cents;
        const char* note = note_find_closest(freq, &cents);

        if (strcmp(note, pending_note) == 0) {
            // Same note again — increment stability counter, capped to avoid overflow
            if (pending_count < NOTE_STABILITY_FRAMES)
                pending_count++;
        } else {
            // New candidate — reset and wait for it to stabilise
            strncpy(pending_note, note, sizeof(pending_note) - 1);
            pending_note[sizeof(pending_note) - 1] = '\0';
            pending_count = 1;
        }

        // Only publish once the note has been stable for enough frames.
        // After that, keep publishing every frame so freq/cents stay live.
        if (pending_count >= NOTE_STABILITY_FRAMES) {
            web_server_update_note(note, freq, cents);
            ESP_LOGI("note", "NOTE: %s  %.2f Hz cent: %.2f", note, freq, cents);
        }
    } else {
        no_detection_count++;
        if (no_detection_count > 20) {
            last_freq = 0.0f;
            pending_count = 0;
            pending_note[0] = '\0';
            no_detection_count = 0;
        }
    }
}

void note_init(void) {
    last_freq = 0.0f;
    no_detection_count = 0;
    pending_count = 0;
    pending_note[0] = '\0';
}
