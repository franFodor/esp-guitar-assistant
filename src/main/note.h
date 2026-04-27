#ifndef NOTE_H
#define NOTE_H

/**
 * @file note.h
 * @brief Single-note detection via FFT and Harmonic Product Spectrum (HPS).
 *
 * Processes half-spectrum magnitude arrays produced by a Hann-windowed
 * 4096-point FFT at 16 kHz.  The dominant fundamental is identified with
 * HPS and an octave-error correction heuristic, then matched against a
 * chromatic reference table spanning E2–C6.  Stable results are published
 * to the web server via web_server_update_note().
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"

/** ADC sample rate in Hz. */
#define SAMPLE_RATE  16000

/** FFT window length in samples.  Frequency resolution ≈ SAMPLE_RATE / FFT_SIZE ≈ 3.9 Hz/bin. */
#define FFT_SIZE     4096

/**
 * @brief Entry in the chromatic reference table.
 */
typedef struct {
    const char *name;      /**< Note name string, e.g. "A4". */
    float       frequency; /**< Reference frequency in Hz. */
} note_t;

/**
 * @brief Initialise internal note-detection state.
 *
 * Resets the stability counter, pending note, and last-frequency fields.
 * Must be called once before the first call to note_frequency_analysis().
 */
void note_init(void);

/**
 * @brief Detect the fundamental frequency from a magnitude spectrum.
 *
 * Applies HPS across four harmonics to @p magnitudes, locates the dominant
 * peak with sub-bin quadratic interpolation, applies an octave-correction
 * pass, and publishes the result via web_server_update_note() once
 * NOTE_STABILITY_FRAMES consecutive frames agree on the same note.
 *
 * @param magnitudes  Half-spectrum magnitude buffer (FFT_SIZE/2 elements)
 *                    from a Hann-windowed 4096-point FFT.  Contents are
 *                    modified in-place (compressed with a 0.65 power curve).
 * @param hps         Scratch buffer for HPS output (FFT_SIZE/2 elements).
 *                    Caller owns the memory; contents are overwritten.
 */
void note_frequency_analysis(float *magnitudes, float *hps);

/**
 * @brief Find the chromatic-scale note closest to a given frequency.
 *
 * Performs a linear scan of the internal reference table and returns the
 * entry with the smallest absolute frequency distance.
 *
 * @param frequency     Detected fundamental frequency in Hz.
 * @param cents_offset  Output: signed deviation in cents from the matched
 *                      reference pitch (positive = sharp, negative = flat).
 * @return Pointer to the matched note-name string (e.g. "E4").
 *         Points into static storage — do not modify or free.
 */
const char *note_find_closest(float frequency, float *cents_offset);

#endif // NOTE_H
