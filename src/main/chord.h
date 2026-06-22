#ifndef CHORD_H
#define CHORD_H

/**
 * @file chord.h
 * @brief Polyphonic chord detection via pitch-class energy accumulation.
 *
 * Accumulates pitch-class energy over FRAMES_TO_ACCUMULATE consecutive FFT
 * frames, L2-normalises the resulting chroma vector, and matches it against
 * shifted major and minor chord templates using cosine similarity.
 * A match is accepted when the best score exceeds CHORD_THRESHOLD and
 * CHORD_STABILITY_FRAMES consecutive windows agree on the same chord.
 */

#include <stdint.h>

/** FFT size — must match the FFT_SIZE in main.c. */
#define CHORD_FFT_SIZE        4096

/** Sample rate assumed when mapping FFT bins to frequencies, in Hz. */
#define CHORD_SAMPLE_RATE     16000

/** Lower frequency bound for pitch-class accumulation, in Hz. */
#define CHORD_MIN_FREQ        60.0f

/** Upper frequency bound for pitch-class accumulation, in Hz. */
#define CHORD_MAX_FREQ        1400.0f

/** Number of pitch classes in one octave (chromatic scale). */
#define NUM_PITCH_CLASSES     12

/** Number of FFT frames accumulated before attempting a chord match. */
#define FRAMES_TO_ACCUMULATE  5

/**
 * @brief Consecutive detection windows required before publishing.
 *
 * Each window is FRAMES_TO_ACCUMULATE hops (~640 ms at 16 kHz / HOP 2048).
 * Two windows → ~1.3 s first-detection latency; prevents single-frame bursts.
 */
#define CHORD_STABILITY_FRAMES 1

/**
 * @brief Cosine similarity threshold for chord acceptance [0, 1].
 *
 * Perfect chord → ~1.0.  Flat noise → ~0.50.
 * 0.70 accepts clean chords while rejecting ambient noise patterns.
 */
#define CHORD_THRESHOLD       0.65f

/**
 * @brief Time-domain RMS threshold below which chord detection is skipped.
 *
 * Applied to the raw float ring-buffer samples (normalised to [-1, 1]).
 * Match the SILENCE_THRESHOLD in main.c or set slightly higher to keep
 * quiet guitar-body resonance from triggering detections.
 */
#define CHORD_AMPLITUDE_THRESHOLD  0.008f

/** Maximum number of individual notes stored in a chord result. */
#define MAX_CHORD_NOTES 4

/**
 * @brief Approximate INMP441 low-frequency -3 dB point used for pre-emphasis.
 * Tune up if low-string chords are still biased; tune down if treble strings
 * are pulled flat.
 */
#define MIC_FC_CHORD  150.0f

/** Chromatic note names indexed by pitch class (0 = C). */
static const char *NOTE_NAMES[NUM_PITCH_CLASSES] = {
    "C", "C#", "D", "D#", "E", "F",
    "F#", "G", "G#", "A", "A#", "B"
};

/**
 * @brief Major chord template (root=1.0, major third=0.8, perfect fifth=0.7).
 * Indices are semitones above the root; all other entries are zero.
 */
static const float MAJOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 0.0f, 0.8f, 0.0f,
    0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f
};

/**
 * @brief Minor chord template (root=1.0, minor third=0.8, perfect fifth=0.7).
 */
static const float MINOR_TEMPLATE[NUM_PITCH_CLASSES] = {
    1.0f, 0.0f, 0.0f, 0.8f, 0.0f, 0.0f,
    0.0f, 0.7f, 0.0f, 0.0f, 0.0f, 0.0f
};

/**
 * @brief Initialise the chord-detection module.
 *
 * Resets the frame accumulator, frame counter, and stability counter.
 * Must be called once before the first call to chord_detect().
 */
void chord_init(void);

/**
 * @brief Detect a chord from one FFT magnitude frame.
 *
 * Applies INMP441 pre-emphasis and power compression to @p magnitudes,
 * then accumulates pitch-class energy into an internal buffer.  Once
 * FRAMES_TO_ACCUMULATE frames have been collected the chroma vector is
 * L2-normalised, matched against all 24 major/minor templates via cosine
 * similarity, and — if the best score exceeds CHORD_THRESHOLD for
 * CHORD_STABILITY_FRAMES consecutive windows — pushes the result to the
 * web server.  The accumulator is reset after each window.
 *
 * @param magnitudes    Half-spectrum magnitude array (CHORD_FFT_SIZE/2 elements).
 *                      Modified in-place (pre-emphasis + power compression).
 * @param audio_samples Time-domain ring-buffer (CHORD_FFT_SIZE float samples,
 *                      normalised to [-1, 1]).  Used for the RMS silence gate.
 */
void chord_detect(float *magnitudes, float *audio_samples);

#endif // CHORD_H
