#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "dsps_fft2r.h"
#include "dsps_wind.h"
#include "chord.h"

static const char* TAG = "chord";

// Accumulated pitch class energy over multiple frames
static float pitch_class_accumulator[NUM_PITCH_CLASSES] = {0};
static int frame_count = 0;


static int freq_to_pitch_class(float freq) {
    // MIDI note number: 69 + 12 * log2(freq / 440)
    float midi = 69.0f + 12.0f * log2f(freq / 440.0f);
    // Round to nearest semitone
    int note_number = (int)(midi + 0.5f);
    // Convert to pitch class (0-11)
    int pitch_class = note_number % 12;
    if (pitch_class < 0) {
        pitch_class += 12;
    }
    return pitch_class;
}

static void roll_template(const float* template, int shift, float* output) {
    for (int i = 0; i < NUM_PITCH_CLASSES; i++) {
        output[(i + shift) % NUM_PITCH_CLASSES] = template[i];
    }
}

static float dot_product(const float* a, const float* b) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_PITCH_CLASSES; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

static float find_max(const float* arr, int size) {
    float max_val = arr[0];
    for (int i = 1; i < size; i++) {
        if (arr[i] > max_val) {
            max_val = arr[i];
        }
    }
    return max_val;
}

void chord_detect(float* magnitudes, float* audio_samples, chord_result_t* result) {
    // Initialize result as invalid
    result->valid = 0;
    result->name[0] = '\0';

    // Accumulate pitch class energy over multiple frames
    float pitch_energy[NUM_PITCH_CLASSES] = {0};

    // Build pitch-class energy vector from magnitude spectrum
    int half_size = CHORD_FFT_SIZE / 2;
    float freq_resolution = (float)CHORD_SAMPLE_RATE / CHORD_FFT_SIZE;

    for (int k = 0; k < half_size; k++) {
        float freq = k * freq_resolution;

        // Filter frequency range (70 Hz - 1500 Hz)
        if (freq < CHORD_MIN_FREQ || freq > CHORD_MAX_FREQ) {
            continue;
        }

        float mag = magnitudes[k];
        int pc = freq_to_pitch_class(freq);
        pitch_energy[pc] += mag;
    }

    // Accumulate over multiple frames for stability
    for (int i = 0; i < NUM_PITCH_CLASSES; i++) {
        pitch_class_accumulator[i] += pitch_energy[i];
    }
    frame_count++;

    // Only detect after accumulating enough frames
    if (frame_count < FRAMES_TO_ACCUMULATE) {
        return;
    }

    // Use accumulated energy
    memcpy(pitch_energy, pitch_class_accumulator, sizeof(pitch_energy));

    // Normalize pitch energy
    float max_val = find_max(pitch_energy, NUM_PITCH_CLASSES);
    if (max_val > 0.0f) {
        for (int i = 0; i < NUM_PITCH_CLASSES; i++) {
            pitch_energy[i] /= max_val;
        }
    }

    // Find strongest 3 pitch classes (for debugging)
    int sorted_indices[NUM_PITCH_CLASSES];
    for (int i = 0; i < NUM_PITCH_CLASSES; i++) {
        sorted_indices[i] = i;
    }

    // Simple bubble sort for top 3
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < NUM_PITCH_CLASSES; j++) {
            if (pitch_energy[sorted_indices[j]] > pitch_energy[sorted_indices[i]]) {
                int temp = sorted_indices[i];
                sorted_indices[i] = sorted_indices[j];
                sorted_indices[j] = temp;
            }
        }
    }

    ESP_LOGD(TAG, "Top pitch classes: %s, %s, %s",
             NOTE_NAMES[sorted_indices[0]],
             NOTE_NAMES[sorted_indices[1]],
             NOTE_NAMES[sorted_indices[2]]);

    // Chord matching - try all major and minor chords
    float best_score = 0.0f;
    int best_root = 0;
    int is_major = 1;

    float rolled_template[NUM_PITCH_CLASSES];

    for (int root = 0; root < NUM_PITCH_CLASSES; root++) {
        roll_template(MAJOR_TEMPLATE, root, rolled_template);
        float major_score = dot_product(pitch_energy, rolled_template);

        if (major_score > best_score) {
            best_score = major_score;
            best_root = root;
            is_major = 1;
        }

        roll_template(MINOR_TEMPLATE, root, rolled_template);
        float minor_score = dot_product(pitch_energy, rolled_template);

        if (minor_score > best_score) {
            best_score = minor_score;
            best_root = root;
            is_major = 0;
        }
    }

    if (best_score >= CHORD_THRESHOLD) {
        result->valid = 1;
        if (is_major) {
            snprintf(result->name, sizeof(result->name), "%s major", NOTE_NAMES[best_root]);
        } else {
            snprintf(result->name, sizeof(result->name), "%s minor", NOTE_NAMES[best_root]);
        }
        
        // Populate individual notes based on chord type
        result->note_count = 3;
        strncpy(result->notes[0], NOTE_NAMES[best_root], 7);
        result->notes[0][7] = '\0';
        
        if (is_major) {
            // Major chord: root, major third (4 semitones), perfect fifth (7 semitones)
            strncpy(result->notes[1], NOTE_NAMES[(best_root + 4) % 12], 7);
            strncpy(result->notes[2], NOTE_NAMES[(best_root + 7) % 12], 7);
        } else {
            // Minor chord: root, minor third (3 semitones), perfect fifth (7 semitones)
            strncpy(result->notes[1], NOTE_NAMES[(best_root + 3) % 12], 7);
            strncpy(result->notes[2], NOTE_NAMES[(best_root + 7) % 12], 7);
        }
        result->notes[1][7] = '\0';
        result->notes[2][7] = '\0';
        
        ESP_LOGI(TAG, "Chord: %s (Notes: %s, %s, %s)", 
                 result->name, result->notes[0], result->notes[1], result->notes[2]);
    }

    // Reset accumulator
    memset(pitch_class_accumulator, 0, sizeof(pitch_class_accumulator));
    frame_count = 0;
}

void chord_init(void) {
    memset(pitch_class_accumulator, 0, sizeof(pitch_class_accumulator));
    frame_count = 0;
}
