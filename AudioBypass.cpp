#include "AudioBypass.h"

void AudioBypass::update(void) {
  audio_block_t *in1 = receiveReadOnly(0);
  audio_block_t *in2 = receiveReadOnly(1);
  if (in1) {
    transmit(in1, 0);
    transmit(in2, 1);
    release(in1);
    release(in2);
  }
}

