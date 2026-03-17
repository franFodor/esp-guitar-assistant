#ifndef CHORD_H
#define CHORD_H

#include <stdint.h>

#define CHORD_FFT_SIZE        2048
#define CHORD_SAMPLE_RATE     16000
#define CHORD_MIN_FREQ        70.0f
#define CHORD_MAX_FREQ        1500.0f

#define NUM_PITCH_CLASSES     12
#define FRAMES_TO_ACCUMULATE  5
#define CHORD_THRESHOLD       0.4f

// Audio level threshold for chord detection (RMS)
// Adjust this value based on your microphone sensitivity
// Typical values: 0.01 (sensitive) to 0.1 (requires loud strumming)
#define CHORD_AMPLITUDE_THRESHOLD  0.002f

static const char* NOTE_NAMES[NUM_PITCH_CLASSES] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

// index 0 - root, index 4 - major third, index 7 - perfect fifth
static const float MAJOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

// index 0 - root, index 3 - minor third, index 7 - perfect fifth
static const float MINOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f
};

#define MAX_CHORD_NOTES 4

/**
 * Chord detection result
 */
typedef struct {
    char name[32];      // Chord name (e.g., "C major", "D# minor")
    int valid;          // 1 if valid chord detected, 0 otherwise
    float amplitude;    // Detected audio amplitude (RMS)
    char notes[MAX_CHORD_NOTES][8];  // Individual notes (e.g., "C", "E", "G")
    int note_count;     // Number of notes in the chord
} chord_result_t;

/**
 * Initialize the chord detection module
 */
void chord_init(void);

/**
 * Perform chord detection on magnitude spectrum
 * @param magnitudes Magnitude spectrum array (CHORD_FFT_SIZE/2 elements)
 * @param audio_samples Raw audio samples for amplitude calculation
 * @param result Output chord detection result
 */
void chord_detect(float* magnitudes, float* audio_samples, chord_result_t* result);

#endif // CHORD_H
