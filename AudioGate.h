#ifndef _audio_gate_
#define _audio_gate_

#include "AudioStream.h"

#define SAMPLES_PER_MSEC (AUDIO_SAMPLE_RATE_EXACT/1000.0)

class AudioGate : public AudioStream
{
public:
  AudioGate(void) : AudioStream(1, inputQueueArray) {
    threshold(0);
    openTime(0);
  }
  void threshold(float th) {
    if (th > 1) th = 1;
    else if(th < 0) th = 0;
    threshold_value = th * 65536.0f;
  }
  void thresholddb(float db) {
    if (db > 0) db = 0;
    threshold_value = pow(10.0, db / 20.0) * 65536.0f;
  }
  void openTime(float mills) {
    open_count = milliseconds2count(mills);
  }
  virtual void update(void);
private:
  uint16_t milliseconds2count(float milliseconds) {
    if (milliseconds < 0.0) milliseconds = 0.0;
    uint32_t c = ((uint32_t)(milliseconds * SAMPLES_PER_MSEC) + 7) >> 3;
    // if (c > 1103) return 1103; // allow up to 200 ms
    return c;
  }

  audio_block_t *inputQueueArray[1];
  int16_t threshold_value;
  int16_t open_count;
  int16_t state;
  int16_t counter;
};

#endif

