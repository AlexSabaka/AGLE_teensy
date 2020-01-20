#ifndef _device_settings_h_
#define _device_settings_h_

struct device_settings
{
  // Filter settings
  bool      filter;                 // are filter enabled
  int       filter_type;            // filter type(lowpass, bandpass)
  int       cutoff;                 // cutoff frequency

  // Logging settings
  bool      log;                    // are logging to the .csv file enabled
  bool      write_wav;              // do write recorded signals
  char      file_name[11];          // .csv file name

  // Processing setting
  bool      processing_enabled;
  int       sampling_rate;          // 
  int       interp_mode;            // which interpolation type use to refine indexes, if NONE no refining are done
  int       linein_sensetivity;     // ADC linein's sensitivity
  float     comparator_value;       // trigger level (in dB)

  // Device parameters
  float     base_length;            // microphones base length (distance between two microphones)
};

const device_settings default_Settings = { false, LOWPASS, 800,
                                           true, true, "LOG.CSV",
                                           true, 44100, INTERP_LINEAR, 8, 0.1,
                                           0.34 };

#endif