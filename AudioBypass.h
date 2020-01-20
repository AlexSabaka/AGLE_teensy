#ifndef _audio_bypass_
#define _audio_bypass_

#include "AudioStream.h"

class AudioBypass : public AudioStream
{
public:
  AudioBypass(void) : AudioStream(2, inputQueueArray) {
  }
  virtual void update(void);
private:
  audio_block_t *inputQueueArray[2];
};

#endif

