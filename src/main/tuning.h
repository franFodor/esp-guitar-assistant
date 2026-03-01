#ifndef TUNING_H
#define TUNING_H

#include <stdint.h>

#define SAMPLE_RATE     16000
#define FFT_SIZE        4096

typedef struct {
    const char* name;
    float frequency;
} note_t;

/**
 * Initialize the tuning module
 */
void tuning_init(void);

/**
 * Perform frequency analysis on magnitude spectrum
 * @param magnitudes Magnitude spectrum array (FFT_SIZE/2 elements)
 * @param hps Working buffer for harmonic product spectrum (FFT_SIZE/2 elements)
 */
void tuning_frequency_analysis(float* magnitudes, float* hps);

/**
 * Find the closest musical note to a given frequency
 * @param frequency Input frequency in Hz
 * @param cents_offset Output parameter for cents offset from the note
 * @return Name of the closest note
 */
const char* tuning_find_closest_note(float frequency, float* cents_offset);

#endif // TUNING_H
