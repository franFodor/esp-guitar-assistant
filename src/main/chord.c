#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "chord.h"
#include "server.h"

static const char *TAG = "chord";

static float pitch_class_accumulator[NUM_PITCH_CLASSES] = {0};
static int   frame_count   = 0;
static char  pending_chord[32] = "";
static int   pending_count = 0;

static int freq_to_pitch_class(float freq) {
    float midi = 69.0f + 12.0f * log2f(freq / 440.0f);
    int note_number = (int)(midi + 0.5f);
    int pitch_class = note_number % 12;
    if (pitch_class < 0) pitch_class += 12;
    return pitch_class;
}

static void roll_template(const float *template, int shift, float *output) {
    for (int i = 0; i < NUM_PITCH_CLASSES; i++)
        output[(i + shift) % NUM_PITCH_CLASSES] = template[i];
}

static float dot_product(const float *a, const float *b) {
    float sum = 0.0f;
    for (int i = 0; i < NUM_PITCH_CLASSES; i++)
        sum += a[i] * b[i];
    return sum;
}

void chord_detect(float *magnitudes, float *audio_samples) {
    int   half_size = CHORD_FFT_SIZE / 2;                         // 2048
    float freq_res  = (float)CHORD_SAMPLE_RATE / CHORD_FFT_SIZE;  // ~3.906 Hz/bin

    // Time-domain RMS from the ring-buffer — calibrated on the same [-1,1] scale
    // as the SILENCE_THRESHOLD in main.c, unlike a spectrum-domain RMS which is
    // on a completely different scale and was far too easy to pass.
    float rms = 0.0f;
    for (int i = 0; i < CHORD_FFT_SIZE; i++) {
        float s = audio_samples[i];
        rms += s * s;
    }
    rms = sqrtf(rms / CHORD_FFT_SIZE);

    if (rms < CHORD_AMPLITUDE_THRESHOLD) {
        memset(pitch_class_accumulator, 0, sizeof(pitch_class_accumulator));
        frame_count   = 0;
        pending_count = 0;
        return;
    }

    // Pre-emphasis: inverse of INMP441's 1st-order HPF — same formula as note.c.
    // Without this, the 82–110 Hz fundamentals of the low strings are suppressed
    // and their 2nd harmonics inflate the wrong pitch classes.
    for (int i = 1; i < half_size; i++) {
        float f = i * freq_res;
        if (f < 500.0f)
            magnitudes[i] *= sqrtf(1.0f + (MIC_FC_CHORD / f) * (MIC_FC_CHORD / f));
    }

    // Power compression — prevents one loud string from drowning all others.
    for (int i = 1; i < half_size; i++)
        magnitudes[i] = powf(magnitudes[i] + 1e-20f, 0.65f);

    // Accumulate pitch-class energy across the search window.
    // Pitch class collapses octaves: C2, C3, C4 all land on class 0.
    float pitch_energy[NUM_PITCH_CLASSES] = {0};
    for (int k = 1; k < half_size; k++) {
        float freq = k * freq_res;
        if (freq < CHORD_MIN_FREQ || freq > CHORD_MAX_FREQ) continue;
        pitch_energy[freq_to_pitch_class(freq)] += magnitudes[k];
    }
    for (int i = 0; i < NUM_PITCH_CLASSES; i++)
        pitch_class_accumulator[i] += pitch_energy[i];
    frame_count++;

    if (frame_count < FRAMES_TO_ACCUMULATE) return;

    // Copy accumulated energy and reset for the next window.
    memcpy(pitch_energy, pitch_class_accumulator, sizeof(pitch_energy));
    memset(pitch_class_accumulator, 0, sizeof(pitch_class_accumulator));
    frame_count = 0;

    // L2-normalise: makes the chord score a proper cosine similarity in [0,1].
    // Old approach (max-normalise) gave flat noise a free 1.0 on the dominant
    // pitch class, so every template always scored its root weight — indistinguishable
    // from an actual chord. Cosine similarity gives flat noise ~0.50, a real chord ~0.85+.
    float l2_sum = 0.0f;
    for (int i = 0; i < NUM_PITCH_CLASSES; i++)
        l2_sum += pitch_energy[i] * pitch_energy[i];
    float l2_norm = sqrtf(l2_sum + 1e-30f);
    for (int i = 0; i < NUM_PITCH_CLASSES; i++)
        pitch_energy[i] /= l2_norm;

    // Template L2 norm — both templates have the same three-weight structure.
    // sqrt(1.0^2 + 0.8^2 + 0.7^2) = sqrt(2.13) ≈ 1.456
    static const float TEMPLATE_NORM = 1.4560f;

    float best_score = 0.0f;
    int   best_root  = 0;
    int   is_major   = 1;
    float rolled[NUM_PITCH_CLASSES];

    for (int root = 0; root < NUM_PITCH_CLASSES; root++) {
        roll_template(MAJOR_TEMPLATE, root, rolled);
        float s = dot_product(pitch_energy, rolled) / TEMPLATE_NORM;
        if (s > best_score) { best_score = s; best_root = root; is_major = 1; }

        roll_template(MINOR_TEMPLATE, root, rolled);
        s = dot_product(pitch_energy, rolled) / TEMPLATE_NORM;
        if (s > best_score) { best_score = s; best_root = root; is_major = 0; }
    }

    ESP_LOGD(TAG, "best: %s %s  score: %.3f",
             NOTE_NAMES[best_root], is_major ? "major" : "minor", best_score);

    if (best_score < CHORD_THRESHOLD) {
        pending_count = 0;
        return;
    }

    char chord_name[32];
    if (is_major)
        snprintf(chord_name, sizeof(chord_name), "%s major", NOTE_NAMES[best_root]);
    else
        snprintf(chord_name, sizeof(chord_name), "%s minor", NOTE_NAMES[best_root]);

    if (strcmp(chord_name, pending_chord) == 0) {
        if (pending_count < CHORD_STABILITY_FRAMES)
            pending_count++;
    } else {
        strncpy(pending_chord, chord_name, sizeof(pending_chord) - 1);
        pending_chord[sizeof(pending_chord) - 1] = '\0';
        pending_count = 1;
    }

    if (pending_count >= CHORD_STABILITY_FRAMES) {
        char notes[3][8];
        strncpy(notes[0], NOTE_NAMES[best_root], 7);
        strncpy(notes[1], NOTE_NAMES[(best_root + (is_major ? 4 : 3)) % 12], 7);
        strncpy(notes[2], NOTE_NAMES[(best_root + 7) % 12], 7);
        notes[0][7] = notes[1][7] = notes[2][7] = '\0';

        web_server_update_chord(chord_name, (const char (*)[8])notes, 3);
        ESP_LOGI(TAG, "Chord: %s (Notes: %s, %s, %s)  score: %.2f",
                 chord_name, notes[0], notes[1], notes[2], best_score);
    }
}

void chord_init(void) {
    memset(pitch_class_accumulator, 0, sizeof(pitch_class_accumulator));
    frame_count   = 0;
    pending_count = 0;
    pending_chord[0] = '\0';
}
