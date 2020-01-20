#ifndef simple_ch_rec_h_
#define simple_ch_rec_h_

#include <vector>
#include <tuple>

#include "utility/dspinst.h"
#include "arm_math.h"

#include "AudioStream.h"
#include "MemorySPI.h"
#include "cubic_spline.h"


#define SAMPLES_PER_MSEC (AUDIO_SAMPLE_RATE_EXACT / 1000.0)

#define MAX16 (4.0f * 4096.0f)

#define SW_NONE 0
#define SW_LEFT 1
#define SW_RIGHT 2

// TODO: Replace defines with enums
#define CHANNELS 2
#define LEFT_CHANNEL 0
#define RIGHT_CHANNEL 1

#define INTERP_NONE 0
#define INTERP_LINEAR 1
#define INTERP_SPLINE 2
#define INTERP_LAGRANGE 3
#define INTERP_FARROW 4
#define INTERP_PARABOLIC 5
#define APPROX_QUADRATIC_OLS 6

#define CROSS_RISE 0
#define CROSS_FALL 1
#define CROSS_BOTH 2

#define MAX_QUEUE_LENGTH 340
#define QUEUE_LENGTH 180 /* 500 ms, recomended value: 300-320 */
#define ZERO_BLOCK 5

#define STATE_IDLE -1
#define STATE_WAIT_SW 0
#define STATE_TRANSITIVE 1
#define STATE_WAIT_BW 2
#define STATE_PLAY 3
#define STATE_BYPASS 4
#define STATE_ENDED 5

#define FIR_MAX_COEFFS 200


class GateRec : public AudioStream
{
public:
  GateRec(void) : AudioStream(CHANNELS, inputQueueArray)
  {
    state = STATE_IDLE;
    th_value[LEFT_CHANNEL] = 0;
    th_value[RIGHT_CHANNEL] = 0;

    _errno = 0;

    clear();
  }

  // TODO: Change params to fir_filter
  void setFilter(const int16_t *a, int16_t n) {
    // n must be an even number, 4 or larger
    a_coeffs = a;
    n_coeffs = n;

    // initialize FIR instance (ARM DSP Math Library)
    if (a_coeffs && n <= FIR_MAX_COEFFS) {
      arm_status init_res_left = arm_fir_init_q15(&fir_inst_left, n, (q15_t*)a_coeffs, &state_q15_left[0], AUDIO_BLOCK_SAMPLES);
      arm_status init_res_right = arm_fir_init_q15(&fir_inst_right, n, (q15_t*)a_coeffs, &state_q15_right[0], AUDIO_BLOCK_SAMPLES);
      if (init_res_left != ARM_MATH_SUCCESS || init_res_right != ARM_MATH_SUCCESS) {
        a_coeffs = NULL;
        n_coeffs = 0;
      }
    }
  }

  void enableFilter(void) {
    firEnabled = true;
  }

  void disableFilter(void) {
    firEnabled = false;
  }

  bool filterState(void) {
    return firEnabled;
  }

  void begin(void) {
    clear();
    state = STATE_WAIT_SW;
  }

  int8_t getState(void) {
    return state;
  }

  void threshold(float val_ch1, float val_ch2) {
    if (val_ch1 > 1) val_ch1 = 1;
    if (val_ch1 < 0) val_ch1 = 0;

    if (val_ch2 > 1) val_ch2 = 1;
    if (val_ch2 < 0) val_ch2 = 0;

    th_value[LEFT_CHANNEL] = val_ch1 * MAX16;
    th_value[RIGHT_CHANNEL] = val_ch1 * MAX16;
  }

  void threshold(float val) {
    threshold(val, val);
  }

  void play(void) {
    state = STATE_PLAY;
  }

  void bypass(void) {
    state = STATE_BYPASS;
  }

  void idle(void) {
    state = STATE_IDLE;
  }

  int8_t err(void) {
    int16_t res =_errno;
    _errno = 0;
    return res;
  }

  int16_t readCurrentMax(void) {
    return _max[LEFT_CHANNEL] > _max[RIGHT_CHANNEL] ? _max[LEFT_CHANNEL] : _max[RIGHT_CHANNEL];
  }

  uint32_t bufferLength(void) {
    return QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES;
  }

  float32_t groupDelay(void) {
    return n_coeffs / (AUDIO_SAMPLE_RATE_EXACT);
  }

  std::vector<float32_t>* refineIndexes(int8_t channel, float32_t value, int8_t interpolation_mode, int8_t crossing_mode) {   
    if (channel >= CHANNELS) return nullptr;

    uint32_t length = QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES;
    std::vector<float32_t> *found = new std::vector<float32_t>();

    uint32_t memory_begin = memory.memoryBegin();
    SPI.beginTransaction(SPISETTING);
    digitalWriteFast(SPIRAM_CS_PIN, LOW);     // begin receiving data

    // memory.read(mem_offset, 1, byte);

    uint32_t mem_offset = ((0 + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * channel;
    uint32_t addr = 2 * (memory_begin + mem_offset);

    SPI.transfer16((0x03 << 8) | (addr >> 16)); // Set higt(addr)
    SPI.transfer16(addr & 0xFFFF);        // Set low(addr)

    int16_t pprev = (int16_t)(SPI.transfer16(0)); // Gets 16 bits from SPI
    int16_t prev = (int16_t)(SPI.transfer16(0)); // Gets 16 bits from SPI
    
    for (uint32_t i = 2; i < length; ++i) {

      // mem_offset = ((i + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * channel;
      // addr = 2 * (memory_begin + mem_offset);

      // SPI.transfer16((0x03 << 8) | (addr >> 16));
      // SPI.transfer16(addr & 0xFFFF);
      int16_t curr = (int16_t)(SPI.transfer16(0)); 

      if (curr > value && prev < value) {
        // TODO: Refactor condition
        if (!((crossing_mode == CROSS_RISE && curr > prev) ||
            (crossing_mode == CROSS_FALL && curr < prev) ||
            (crossing_mode == CROSS_BOTH))) break;

        float32_t curr_i = 0;
        switch (interpolation_mode) {
          case INTERP_NONE: curr_i = i - 1; break;
          case INTERP_LINEAR: {
            curr_i =  ((value - prev) / (float32_t)(curr - prev)) + i - 1;
          } break;
          case INTERP_PARABOLIC: {
            // uint32_t mem_offset_pp = ((((i - 2) > 0 ? (i - 2) : 0) + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * channel;
            // uint32_t addr_pp = 2 * (memory_begin + mem_offset_pp);

            // SPI.transfer16((0x03 << 8) | (addr_pp >> 16));
            // SPI.transfer16(addr_pp & 0xFFFF);
            // int16_t pprev = (int16_t)(SPI.transfer16(0)); // read(channel, (i - 2) > 0 ? (i - 2) : 0);

            float32_t frac1 = (value - prev) * (value - curr) / (float32_t)((pprev - prev) * (pprev - curr));
            float32_t frac2 = (value - pprev) * (value - curr) / (float32_t)((prev - pprev) * (prev - curr));
            float32_t frac3 = (value - pprev) * (value - prev) / (float32_t)((curr - pprev) * (curr - prev));

            curr_i = frac1 * (i - 2) + frac2 * (i - 1) + frac3 * i;
          } break;
          case INTERP_SPLINE: {
            //TODO: Add splines support, and so on for next...
            curr_i =  i - 1;
          } break;
          case INTERP_LAGRANGE: curr_i =  i - 1; break;
          case INTERP_FARROW: curr_i =  i - 1; break;
          case APPROX_QUADRATIC_OLS: {
            // TODO: seems it doesn't works properly :(
            // Count of points to be approximated equal 2*n+1
            curr_i =  i - 1;
          } break;
        }
        found->push_back(curr_i);
      }
      pprev = prev;  // <--- эт я сделяль.
      prev = curr;
    }

    digitalWriteFast(SPIRAM_CS_PIN, HIGH);    // end receiving data
    SPI.endTransaction();
    return found;
  }

  std::tuple<uint32_t, int16_t> getPeak(int8_t channel, int8_t p) {
    if (channel >= CHANNELS) return std::make_tuple(-1, -1);
    if (p == 0) return std::make_tuple(peak1_i[channel], _max[channel]);
    else if (p == 1) return std::make_tuple(peak2_i[channel], _max_bw[channel]);
    else return std::make_tuple(-1, -1);
  }

  uint16_t getQueueHeadIndex(void) {
    return head;
  }

  int16_t read(int8_t channel, uint32_t index);
  void clear(void);
  virtual void update(void);
private:

  MemorySPI             memory;                                                 // instanes of SPI memory manager
  audio_block_t         *inputQueueArray[CHANNELS];                             // DMA buffers for input signal
  uint16_t              filterSwitchDelay = (uint16_t)(0.5 * SAMPLES_PER_MSEC); // 500 us delay before filter will be switched on 
  uint8_t               hitCounter;                                             // TODO: Remove this field
  uint16_t              current_block;                                          // TODO: Refactor this field
  uint16_t              state;                                                  // Current state
  uint16_t              th_value[CHANNELS];                                     // thresholds values
  uint16_t              _max[CHANNELS];                                         // maximum SW values in each channel
  uint16_t              _max_bw[CHANNELS];                                      // maximum BW values in each channel
  uint16_t              tail;                                                   // tail block index
  uint16_t              head;                                                   // head block index
  uint32_t              peak1_i[CHANNELS];                                      // SW peaks(maximums) indexes by channel
  uint32_t              peak2_i[CHANNELS];                                      // BW peaks(maximums) indexes by channel
  uint8_t               _errno;                                                 // Stores error code
  uint8_t               firstWave;                                              // Stores channel which recieves SW first

  const int16_t         *a_coeffs;                                              // FIR filter coefficients
  int16_t               n_coeffs;                                               // Count of coefficients
  bool                  firEnabled;                                             // are filter enabled

  // ARM DSP Math library filter instance
  arm_fir_instance_q15  fir_inst_left;
  arm_fir_instance_q15  fir_inst_right;
  q15_t                 state_q15_left[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
  q15_t                 state_q15_right[AUDIO_BLOCK_SAMPLES + FIR_MAX_COEFFS];
};

#endif
