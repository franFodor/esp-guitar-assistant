#ifndef CHORD_H
#define CHORD_H

/**
 * @file chord.h
 * @brief Polyphonic chord detection via pitch-class energy accumulation.
 *
 * Accumulates pitch-class energy over FRAMES_TO_ACCUMULATE consecutive FFT
 * frames, normalises the resulting chroma vector, and matches it against
 * shifted major and minor chord templates using a dot-product score.
 * A match is accepted when the best score exceeds CHORD_THRESHOLD.
 */

#include <stdint.h>

/** FFT size used by the chord detector (independent of the note FFT size). */
#define CHORD_FFT_SIZE        2048

/** Sample rate assumed when mapping FFT bins to frequencies, in Hz. */
#define CHORD_SAMPLE_RATE     16000

/** Lower frequency bound for pitch-class accumulation, in Hz. */
#define CHORD_MIN_FREQ        70.0f

/** Upper frequency bound for pitch-class accumulation, in Hz. */
#define CHORD_MAX_FREQ        1500.0f

/** Number of pitch classes in one octave (chromatic scale). */
#define NUM_PITCH_CLASSES     12

/** Number of FFT frames accumulated before attempting a chord match. */
#define FRAMES_TO_ACCUMULATE  5

/**
 * @brief Minimum dot-product score required to accept a chord match.
 *
 * Range 0–3 (sum of three template weights).  Lower values increase
 * sensitivity but also false-positive rate.
 */
#define CHORD_THRESHOLD       0.85f

/**
 * @brief RMS amplitude threshold below which chord detection is skipped.
 *
 * Adjust based on microphone gain.  Typical range: 0.01 (sensitive) to
 * 0.1 (requires loud strumming).
 */
#define CHORD_AMPLITUDE_THRESHOLD  0.008f

/** Maximum number of individual notes stored in a chord_result_t. */
#define MAX_CHORD_NOTES 4

/** Chromatic note names indexed by pitch class (0 = C). */
static const char *NOTE_NAMES[NUM_PITCH_CLASSES] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

/**
 * @brief Major chord template (root, major third, perfect fifth).
 *
 * Indices represent semitones above the root; non-zero entries are chord tones.
 */
static const float MAJOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f,
    0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f
};

/**
 * @brief Minor chord template (root, minor third, perfect fifth).
 *
 * Indices represent semitones above the root; non-zero entries are chord tones.
 */
static const float MINOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f,
    0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f
};

/**
 * @brief Initialise the chord-detection module.
 *
 * Resets the frame accumulator and frame counter.  Must be called once
 * before the first call to chord_detect().
 */
void chord_init(void);

/**
 * @brief Detect a chord from one FFT magnitude frame.
 *
 * Accumulates pitch-class energy from @p magnitudes into an internal buffer.
 * Once FRAMES_TO_ACCUMULATE frames have been collected the chroma vector is
 * normalised, matched against all 24 major/minor templates, and — if the best
 * score exceeds CHORD_THRESHOLD — pushes the result to the web server directly.
 * The accumulator is reset after each attempt regardless of outcome.
 *
 * @param magnitudes    Half-spectrum magnitude array (CHORD_FFT_SIZE/2 elements).
 * @param audio_samples Raw float audio samples (unused, reserved for RMS gating).
 */
void chord_detect(float *magnitudes, float *audio_samples);

#endif // CHORD_H
