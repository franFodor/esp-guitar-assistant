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
#define NOTE_STABILITY_FRAMES 2

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

static float harmonic_product_spectrum(float* magnitudes, int half_size, float* hps, float* confidence_out) {
    float freq_res = (float)SAMPLE_RATE / FFT_SIZE;
    // R=5: five harmonics give much better discrimination between a guitar note
    // (which has strong energy at many harmonics) and power-line hum or other
    // tonal noise (which typically has fewer or weaker high harmonics).
    const int R = 5;

    for (int i = 0; i < half_size; i++) hps[i] = 1.0f;
    for (int r = 1; r <= R; r++) {
        int limit = half_size / r;
        for (int i = 0; i < limit; i++) hps[i] *= (magnitudes[i * r] + 1e-12f);
        for (int i = limit; i < half_size; i++) hps[i] = 0.0f;
    }

    int min_bin = (int)floorf(60.0f / freq_res);
    int max_bin = (int)ceilf(1200.0f / freq_res);
    if (min_bin < 1) min_bin = 1;
    if (max_bin >= half_size / R) max_bin = half_size / R - 1;

    int best_bin = min_bin;
    float best_val = hps[min_bin];
    for (int i = min_bin + 1; i <= max_bin; i++) {
        if (hps[i] > best_val) { best_val = hps[i]; best_bin = i; }
    }

    // Confidence = peak / geometric-mean of the HPS floor in the search range.
    // A true guitar note produces a sharp spike well above the surrounding floor;
    // noise or hum produces a relatively flat HPS with a small peak-to-floor ratio.
    float log_sum = 0.0f;
    for (int i = min_bin; i <= max_bin; i++) log_sum += logf(hps[i] + 1e-30f);
    float hps_floor = expf(log_sum / (float)(max_bin - min_bin + 1));
    *confidence_out = (hps_floor > 1e-30f) ? (best_val / hps_floor) : 0.0f;

    // Octave correction: prefer the lower octave when it has meaningful HPS energy.
    // Threshold 0.35: the MEMS mic attenuates the low-string fundamentals (E2, A2)
    // by several dB, so hps[fundamental] is never close to hps[2nd_harmonic] even
    // when the string is clearly ringing. The pre-emphasis in note_frequency_analysis
    // partially compensates, but 0.35 is needed to catch the remaining cases.
    int lower_bin = best_bin / 2;
    if (lower_bin >= min_bin && hps[lower_bin] >= 0.35f * best_val)
        best_bin = lower_bin;

    return quadratic_interpolation(hps, best_bin);
}

const char* note_find_closest(float frequency, float* cents_offset) {
    const char* note = "Unknown";
    float min_cents = 1e9f;
    for (int i = 0; i < NUM_NOTES; i++) {
        // Use cents (log ratio) so every semitone interval is treated equally
        // regardless of register — a plain Hz difference biases toward low notes.
        float c = fabsf(1200.0f * log2f(frequency / chromatic_scale[i].frequency));
        if (c < min_cents) {
            min_cents = c;
            note = chromatic_scale[i].name;
            *cents_offset = 1200.0f * log2f(frequency / chromatic_scale[i].frequency);
        }
    }
    return note;
}

// Minimum ratio of HPS peak to HPS geometric-mean floor required to process the frame.
#define HPS_CONFIDENCE_THRESHOLD 8.0f
// Maximum |cents| from the nearest chromatic note to accept a detection.
// Hum detections (60-75 Hz vs E2 at 82 Hz) land 150-700 cents away; normally-tuned
// strings are within ±50 cents. 70 gives headroom for a slightly detuned instrument.
#define CENTS_WINDOW 70.0f

// Approximate INMP441 low-frequency -3 dB point. Tune this up if E2/A2 are
// still detected an octave high; tune it down if high strings are pulled flat.
#define MIC_FC 150.0f

void note_frequency_analysis(float* magnitudes, float* hps) {
    // Boost low frequencies to compensate for the INMP441's high-pass roll-off.
    // Without this, the 82 Hz fundamental of the low E string is suppressed by
    // the microphone and the HPS incorrectly finds the 2nd harmonic (164 Hz = E3).
    // The correction is 1/|H_HPF(f)| = sqrt(1 + (fc/f)^2) for a 1st-order HPF.
    float freq_res_local = (float)SAMPLE_RATE / FFT_SIZE;
    for (int i = 1; i < FFT_SIZE / 2; i++) {
        float f = i * freq_res_local;
        if (f < 500.0f)
            magnitudes[i] *= sqrtf(1.0f + (MIC_FC / f) * (MIC_FC / f));
    }

    for (int i = 1; i < FFT_SIZE/2; i++)
        magnitudes[i] = powf(magnitudes[i] + 1e-20f, 0.65f);

    float confidence;
    float freq = harmonic_product_spectrum(magnitudes, FFT_SIZE/2, hps, &confidence);

    // Skip frames with no clear harmonic peak — don't touch pending state so
    // that a brief noise burst between valid frames doesn't break the counter.
    if (confidence < HPS_CONFIDENCE_THRESHOLD) return;

    float cents;
    const char* note = note_find_closest(freq, &cents);

    // Skip frames whose detected frequency is implausibly far from any note.
    // This rejects hum and sub-fundamental artefacts without touching the counter.
    if (fabsf(cents) > CENTS_WINDOW) return;

    // Only update pending state on genuinely valid frames.
    if (strcmp(note, pending_note) == 0) {
        if (pending_count < NOTE_STABILITY_FRAMES)
            pending_count++;
    } else {
        strncpy(pending_note, note, sizeof(pending_note) - 1);
        pending_note[sizeof(pending_note) - 1] = '\0';
        pending_count = 1;
    }

    if (pending_count >= NOTE_STABILITY_FRAMES) {
        web_server_update_note(note, freq, cents);
        ESP_LOGI("note", "NOTE: %s  %.2f Hz cent: %.2f conf: %.1f", note, freq, cents, confidence);
    }
}

void note_init(void) {
    pending_count = 0;
    pending_note[0] = '\0';
}
