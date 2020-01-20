#include "GateRec.h"


void GateRec::clear(void) {
  memory.zero(0, QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES);                   // Clear each channels
  memory.zero(QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES, QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES);
  current_block = 0;

  _max[LEFT_CHANNEL] = INT16_MIN;
  _max[RIGHT_CHANNEL] = INT16_MIN;
  _max_bw[LEFT_CHANNEL] = 0;
  _max_bw[RIGHT_CHANNEL] = 0;

  peak1_i[LEFT_CHANNEL] = 0;
  peak1_i[RIGHT_CHANNEL] = 0;
  peak2_i[LEFT_CHANNEL] = 0;
  peak2_i[RIGHT_CHANNEL] = 0;

  tail = 0;
  head = 0;

  firstWave = SW_NONE;
}

int16_t GateRec::read(int8_t channel, uint32_t index) {
  if (channel > CHANNELS) return 0;

  uint32_t mem_offset = ((index + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * channel;
  int16_t *byte = (int16_t*)malloc(sizeof(int16_t) * 1);
  memory.read(mem_offset, 1, byte);
  int16_t value = *byte;
  free(byte);
  return value;

  // if (index < ZERO_BLOCK * AUDIO_BLOCK_SAMPLES) {
  //   uint32_t mem_offset = (index + tail * AUDIO_BLOCK_SAMPLES) % (ZERO_BLOCK * AUDIO_BLOCK_SAMPLES) + channel * QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES;
  //   int16_t *byte = (int16_t*)malloc(sizeof(int16_t) * 1);
  //   memory.read(mem_offset, 1, byte);
  //   int16_t value = *byte;
  //   free(byte);
  //   return value;
  // } else {
  //   uint32_t mem_offset = (index - ZERO_BLOCK * AUDIO_BLOCK_SAMPLES) + channel * QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES;
  //   int16_t *byte = (int16_t*)malloc(sizeof(int16_t) * 1);
  //   memory.read(mem_offset, 1, byte);
  //   int16_t value = *byte;
  //   free(byte);
  //   return value;
  // }
}

void GateRec::update(void) {
  if (state == STATE_IDLE) return;

  audio_block_t *block_left, *block_right;
  block_left = receiveReadOnly(LEFT_CHANNEL);
  if (block_left) {
    block_right = receiveReadOnly(RIGHT_CHANNEL);
    if (!block_right) {
      _errno = 2;
      release(block_left);
      return;
    }
  } else {
    _errno = 1;
    return;
  }

  switch (state) {
    case STATE_WAIT_SW: {

      memory.write(tail * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_left->data);
      memory.write((tail + QUEUE_LENGTH) * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_right->data);

      int16_t max_val_left = INT16_MIN, max_val_right = INT16_MIN;
      uint32_t max_i_left = 0, max_i_right = 0;

      // TODO: add noise level measuring
      for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
        int16_t curr_left =  block_left->data[i];
        int16_t curr_right =  block_right->data[i];
 
        if (curr_left > max_val_left) { 
          max_val_left = curr_left;
          max_i_left = i;
        }
        if (curr_right > max_val_right) {
          max_val_right = curr_right;
          max_i_right = i;
        }
      }

      _max[LEFT_CHANNEL] = max_val_left;
      _max[RIGHT_CHANNEL] = max_val_right;

      if (max_val_left > th_value[LEFT_CHANNEL]) {
        state = STATE_TRANSITIVE;

        if (firstWave == 0) head = tail - ZERO_BLOCK;

        firstWave |= SW_LEFT;
        peak1_i[LEFT_CHANNEL] =  max_i_left + ZERO_BLOCK * AUDIO_BLOCK_SAMPLES;

        if (head < 0) head = QUEUE_LENGTH + head;
      }
      if (max_val_right > th_value[RIGHT_CHANNEL]) {
        state = STATE_TRANSITIVE;

        if (firstWave == 0) head = tail - ZERO_BLOCK;

        firstWave |= SW_RIGHT;
        peak1_i[RIGHT_CHANNEL] = max_i_right + ZERO_BLOCK * AUDIO_BLOCK_SAMPLES;

        if (head < 0) head = QUEUE_LENGTH + head;
      }

      tail++;
      if (tail >= QUEUE_LENGTH) tail = 0;
    } break;

    case STATE_TRANSITIVE: {
      if (!((firstWave & SW_LEFT) && (firstWave & SW_RIGHT))) {

          memory.write(tail * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_left->data);
          memory.write((tail + QUEUE_LENGTH) * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_right->data);

          int16_t max_val_left = INT16_MIN, max_val_right = INT16_MIN;
          uint32_t max_i_left = 0, max_i_right = 0;

          for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
            int16_t curr_left =  block_left->data[i];
            int16_t curr_right =  block_right->data[i];
     
            if (curr_left > max_val_left) { 
              max_val_left = curr_left;
              max_i_left = i;
            }
            if (curr_right > max_val_right) {
              max_val_right = curr_right;
              max_i_right = i;
            }
          }

          if (firstWave & SW_RIGHT) _max[LEFT_CHANNEL] = max_val_left;
          if (firstWave & SW_LEFT) _max[RIGHT_CHANNEL] = max_val_right;

          if (max_val_left > th_value[LEFT_CHANNEL]) { 
            peak1_i[LEFT_CHANNEL] =  max_i_left + ZERO_BLOCK * AUDIO_BLOCK_SAMPLES;
          }
          if (max_val_right > th_value[RIGHT_CHANNEL]) {
            peak1_i[RIGHT_CHANNEL] = max_i_right + ZERO_BLOCK * AUDIO_BLOCK_SAMPLES;
          }

          tail++;
      }
      
      state = STATE_WAIT_BW;
    } break;

    case STATE_WAIT_BW: {

      int16_t max_val_left = INT16_MIN, max_val_right = INT16_MIN;
      uint32_t max_i_left = 0, max_i_right = 0;

      if (firEnabled) {
        audio_block_t *filtered_left, *filtered_right;

        filtered_left = allocate();
        if (filtered_left) {
          filtered_right = allocate();
          if (!filtered_right) {
            _errno = 4;
            break;
          }
        } else {
          _errno = 3;
          break;
        }

        arm_fir_fast_q15(&fir_inst_left, (q15_t*)block_left->data, (q15_t*)filtered_left->data, AUDIO_BLOCK_SAMPLES);
        arm_fir_fast_q15(&fir_inst_right, (q15_t*)block_right->data, (q15_t*)filtered_right->data, AUDIO_BLOCK_SAMPLES);

        // Finding next maximum value
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
          int16_t curr_left =  (filtered_left->data[i]);
          int16_t curr_right =  (filtered_right->data[i]);

          if (curr_left > max_val_left) {
            max_val_left = curr_left;
            max_i_left = i;
          }
          if (curr_right > max_val_right) {
            max_val_right = curr_right;
            max_i_right = i;
          }
        }

        // Store new data into memory (23LC1024 on SPI)
        memory.write(tail * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, filtered_left->data);
        memory.write((tail + QUEUE_LENGTH) * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, filtered_right->data);

        release(filtered_left);
        release(filtered_right);
      } else {
        // Store new data into memory (23LC1024 on SPI)
        memory.write(tail * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_left->data);
        memory.write((tail + QUEUE_LENGTH) * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, block_right->data);

        // Finding next maximum value
        for (int i = 0; i < AUDIO_BLOCK_SAMPLES; ++i) {
          int16_t curr_left =  (block_left->data[i]);
          int16_t curr_right =  (block_right->data[i]);

          if (curr_left > max_val_left) {
            max_val_left = curr_left;
            max_i_left = i;
          }
          if (curr_right > max_val_right) {
            max_val_right = curr_right;
            max_i_right = i;
          }
        }
      }

      // Last peak has greater priority, so >=
      if (max_val_left >= _max_bw[LEFT_CHANNEL]) {
        _max_bw[LEFT_CHANNEL] = max_val_left;
        peak2_i[LEFT_CHANNEL] = max_i_left + (tail > head ? (tail - head) : (tail + QUEUE_LENGTH - head)) * AUDIO_BLOCK_SAMPLES;

        // compensate group delay if filter enabled 
        if (firEnabled) {
          peak2_i[LEFT_CHANNEL] = peak2_i[LEFT_CHANNEL] - (n_coeffs - 1)/2;
        }
      }

      if (max_val_right >= _max_bw[RIGHT_CHANNEL]) {
        _max_bw[RIGHT_CHANNEL] = max_val_right;
        peak2_i[RIGHT_CHANNEL] = max_i_right + (tail > head ? (tail - head) : (tail + QUEUE_LENGTH - head)) * AUDIO_BLOCK_SAMPLES;
        
        // compensate group delay if filter enabled 
        if (firEnabled) {
          peak2_i[RIGHT_CHANNEL] = peak2_i[RIGHT_CHANNEL] - (n_coeffs - 1)/2;
        }
      }

      tail++;
      if (tail == head) {
        state = STATE_ENDED;
      }
      else if (tail >= QUEUE_LENGTH) {
        tail = 0;
      }
    } break;

    case STATE_PLAY: {

      audio_block_t *buffer_left, *buffer_right;
      buffer_left = allocate();
      if (buffer_left) {
        buffer_right = allocate();
        if (!buffer_right) {
          release(buffer_left);
          break;
        }
      }
      else break;

      memory.read(current_block * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, buffer_left->data);
      memory.read(current_block * AUDIO_BLOCK_SAMPLES + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES, AUDIO_BLOCK_SAMPLES, buffer_right->data);

      transmit(buffer_left, LEFT_CHANNEL);
      transmit(buffer_right, RIGHT_CHANNEL);
      release(buffer_left);
      release(buffer_right);

      current_block++;
      if (current_block >= QUEUE_LENGTH) {
        current_block = 0;
        state = STATE_IDLE;
      }
    } break;

    case STATE_BYPASS: {
      transmit(block_left, LEFT_CHANNEL);
      transmit(block_right, RIGHT_CHANNEL);
    } break;
  }
  
  release(block_left);
  release(block_right);
}