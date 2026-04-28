#include "note.h"
#include "server.h"

static note_t chromatic_scale[] = {
    {"E2", 82.41},  {"F2", 87.31},   {"F#2", 92.50},  {"G2", 98.00},   {"G#2", 103.83},
    {"A2", 110.00}, {"A#2", 116.54}, {"B2", 123.47},  {"C3", 130.81},  {"C#3", 138.59},
    {"D3", 146.83}, {"D#3", 155.56}, {"E3", 164.81},  {"F3", 174.61},  {"F#3", 185.00},
    {"G3", 196.00}, {"G#3", 207.65}, {"A3", 220.00},  {"A#3", 233.08}, {"B3", 246.94},
    {"C4", 261.63}, {"C#4", 277.18}, {"D4", 293.66},  {"D#4", 311.13}, {"E4", 329.63},
    {"F4", 349.23}, {"F#4", 369.99}, {"G4", 392.00},  {"G#4", 415.30}, {"A4", 440.00},
    {"A#4", 466.16},{"B4", 493.88},
    {"C5", 523.25}, {"C#5", 554.37}, {"D5", 587.33},  {"D#5", 622.25}, {"E5", 659.25},
    {"F5", 698.46}, {"F#5", 739.99}, {"G5", 783.99},  {"G#5", 830.61}, {"A5", 880.00},
    {"A#5", 932.33},{"B5", 987.77},  {"C6", 1046.50}
};

#define NUM_NOTES (sizeof(chromatic_scale) / sizeof(chromatic_scale[0]))

// Persistence state: a note must be detected this many consecutive frames before publishing
#define NOTE_STABILITY_FRAMES 3

static char pending_note[8] = "";
static int  pending_count = 0;

static float quadratic_interpolation(float* magnitudes, int peak_bin) {
    if (peak_bin <= 0 || peak_bin >= (FFT_SIZE/2 - 1))
        return (float)peak_bin * SAMPLE_RATE / FFT_SIZE;

    // Fit a parabola to the three bins around the peak to get sub-bin frequency resolution.
    // At 16 kHz / 4096 points each bin is ~3.9 Hz wide — without interpolation that would be
    // the worst-case pitch error, which is audible (a semitone at E2 is only ~4 Hz wide).
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

    // Multiply downsampled copies of the spectrum together (R harmonics).
    // Each harmonic reinforces bins where a true fundamental is present:
    // if bin k is the fundamental, bins 2k, 3k … Rk also have energy, so
    // only the true fundamental survives all R multiplications at full strength.
    // R=4 means we check up to the 4th harmonic (enough for guitar without
    // pulling in unrelated pitches from neighbouring strings).
    for (int i = 0; i < half_size; i++) hps[i] = 1.0f;
    for (int r = 1; r <= R; r++) {
        int limit = half_size / r;
        for (int i = 0; i < limit; i++) hps[i] *= (magnitudes[i * r] + 1e-12f);
        for (int i = limit; i < half_size; i++) hps[i] = 0.0f;
    }

    // Search for the strongest peak within the guitar frequency range (70–1200 Hz
    // covers E2 on the low string up through the high frets of the top string).
    int min_bin = (int)floorf(70.0f / freq_res);
    int max_bin = (int)ceilf(1200.0f / freq_res);
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
    // Compress the dynamic range of the magnitude spectrum before HPS so that
    // quieter harmonics still contribute without being drowned out by loud ones.
    // Exponent 0.65 was chosen empirically: lower values over-boost noise,
    // higher values underweight weak harmonics and hurt HPS accuracy.
    for (int i = 1; i < FFT_SIZE/2; i++)
        magnitudes[i] = powf(magnitudes[i] + 1e-20f, 0.65f);

    float freq = harmonic_product_spectrum(magnitudes, FFT_SIZE/2, hps);

    float cents;
    const char* note = note_find_closest(freq, &cents);
    // cents = 1200 * log2(f / f_reference) — positive means sharp, negative means flat.

    // Require the same note for NOTE_STABILITY_FRAMES consecutive frames
    // before publishing to avoid flickering on transients. A pluck attack
    // often reads a slightly wrong pitch for one or two hops before settling.
    if (strcmp(note, pending_note) == 0) {
        if (pending_count < NOTE_STABILITY_FRAMES)
            pending_count++;
    } else {
        strncpy(pending_note, note, sizeof(pending_note) - 1);
        pending_note[sizeof(pending_note) - 1] = '\0';
        pending_count = 1;
    }

    // Once stable, keep publishing every frame so freq/cents stay live
    if (pending_count >= NOTE_STABILITY_FRAMES) {
        web_server_update_note(note, freq, cents);
        ESP_LOGI("note", "NOTE: %s  %.2f Hz cent: %.2f", note, freq, cents);
    }
}

void note_init(void) {
    pending_count = 0;
    pending_note[0] = '\0';
}
