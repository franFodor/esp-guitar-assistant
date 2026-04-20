#ifndef NOTE_H
#define NOTE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"

#define SAMPLE_RATE     16000
#define FFT_SIZE        4096

typedef struct {
    const char* name;
    float frequency;
} note_t;

void note_init(void);
void note_frequency_analysis(float* magnitudes, float* hps);
const char* note_find_closest(float frequency, float* cents_offset);

#endif // NOTE_H
