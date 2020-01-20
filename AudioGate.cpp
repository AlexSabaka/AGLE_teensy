#include "AudioGate.h"

#define OPENED 1
#define CLOSED 0

void AudioGate::update(void) {
  audio_block_t *in = receiveReadOnly(0);
  if (in) {
    int16_t max = 0;
    const int16_t *p = in->data;
    const int16_t *end = p + AUDIO_BLOCK_SAMPLES;

    do {
      int16_t data = *p++;
      counter++;
      if (abs(data) > max) max = abs(data);
    } while (p < end);
    
    if (max > threshold_value) {
      if (state != OPENED) {
        state = OPENED;
        counter = 0;
      }
      if (state == OPENED) transmit(in);
    }
    if (state == OPENED && counter >= open_count) state = CLOSED;
    release(in);
  }
}

