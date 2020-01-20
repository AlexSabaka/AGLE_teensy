#ifndef _filter_h
#define _filter_h

#include "AudioStream.h"

struct fir_filter {
    int16_t *numerator;
    int16_t order;
};

extern fir_filter low_pass_cut_1000Hz;
extern fir_filter low_pass_cut_800Hz;
extern fir_filter band_pass_1kHz_2kHz;

#endif