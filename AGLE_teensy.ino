#include "stdafx.h"

#define DEBUG

#if DEBUG
#define Serial empt
#else
class E {
public:
  void begin(int) { };
  void println(...) { };
  void printf(...) { };
  void print(...) { };
}; 
E empt;
#endif

/*
 *    TODO:
 *    1. Error & status code out in normal 
 *    2. Add full logging to SD card
 *    3. Rewrite control_sgtl5000.cpp for valuable sample rate configuration
 *    4. [DONE] Change format of LOG.TXT file to CSV file 
 *    5. [GUESS DONE] Correct filter(enable one in the right time when both SW are arrived) 
 *    6. Change ALL DEFINES into ENUMS! Except debug mode defines
 */

AudioInputI2S             i2s1;    
GateRec                   rec;           
AudioOutputI2S            i2s2;           
AudioConnection           patchCord2(i2s1, 0, rec, 0);
AudioConnection           patchCord4(i2s1, 1, rec, 1);
AudioConnection           patchCord5(rec, 0, i2s2, 0);
AudioConnection           patchCord6(rec, 1, i2s2, 1);
AudioControlSGTL5000      sgtl5000_1;     

  /*
     0: 3.12 Volts p-p
     1: 2.63 Volts p-p
     2: 2.22 Volts p-p
     3: 1.87 Volts p-p
     4: 1.58 Volts p-p
     5: 1.33 Volts p-p
     6: 1.11 Volts p-p
     7: 0.94 Volts p-p
     8: 0.79 Volts p-p
     9: 0.67 Volts p-p
    10: 0.56 Volts p-p
    11: 0.48 Volts p-p
    12: 0.40 Volts p-p
    13: 0.34 Volts p-p
    14: 0.29 Volts p-p
    15: 0.24 Volts p-p
  */
float32_t linein_sens[16] = {3.12, 2.63, 2.22, 1.87, 1.58, 1.33, 1.11, 0.94, 0.79, 0.67, 0.56, 0.48, 0.40, 0.34, 0.29, 0.24};

const char* SETTINGS_FILE = "SETTINGS.INI";

DHT tempSensor(A7, DHT22, 30);
Sd2Card card;
SdVolume volume;

device_settings current_settings;

int8_t sd_init_status = 0;

int saveWavFile(void);
void proccessRecordedData(void);
void proccessBearingRecord(void);
int8_t read_settings(device_settings&);
void print_settings(void);
int8_t initialize_sd_card(void);
int8_t initialize_audio_subsystem(void);

void setup() {
  pinMode(LED, OUTPUT);
  digitalWriteFast(LED, HIGH);
  Serial.begin(9600);
  delay(400);

  Serial.printf("%2.2f kB of memory are free.\n", getFreeMemory() / 1000.0);
  Serial.printf("Exact sample rate is %f\n", AUDIO_SAMPLE_RATE_EXACT);

  sd_init_status = initialize_sd_card();

  digitalWriteFast(LED, LOW);
  delay(100);
  digitalWriteFast(LED, HIGH);

  if (sd_init_status & SD_NOT_CONNECTED) Serial.println("SD card not connected. Device wouldn't save audio files.");
  if (sd_init_status & SD_UNABLE_ACCESS_FS) Serial.println("Can't access filesystem.");
  if (sd_init_status == SD_OK) {
    int8_t res = read_settings(current_settings);

    digitalWriteFast(LED, LOW);
    delay(100);
    digitalWriteFast(LED, HIGH);

    switch (res) {
      case S_OK: print_settings(); break;
      case S_NOT_EXISTS: Serial.printf("File \"%s\" not found. Please check if it's exists and named correctly. ", SETTINGS_FILE); goto def_sets;
      case S_INVALID: Serial.printf("File \"%s\" is corrupted or has wrong content. Please check it. ", SETTINGS_FILE); goto  def_sets;
      case S_INCORRECT_VALUE: Serial.printf("Some of parameters in the file \"%s\" are incrorrect. Please fix them. ", SETTINGS_FILE); goto def_sets;
      default: 
      def_sets: Serial.println("Reading default setting..."); current_settings = default_Settings; break;
    }
  }

  int8_t status = initialize_audio_subsystem();

  digitalWriteFast(LED, LOW);
  delay(100);
  digitalWriteFast(LED, HIGH);

  if (status != S_OK) {
    Serial.printf("Audio subsystem can't be initialized. Check if it isn't damaged and connected to the CPU board.\n!!!CANNOT INITIALIZE!!!\n");
    while (true) {
      delay(100);
      digitalWriteFast(LED, LOW);
      delay(30);
      digitalWriteFast(LED, HIGH);
    }
  }

  digitalWriteFast(LED, LOW);
  delay(100);
  digitalWriteFast(LED, HIGH);

  Serial.println("Waiting...");
}

// TODO: Add nice error handling
int8_t read_settings(device_settings &settings) {
  const int buffer_length = 40;
  char buffer[buffer_length] = { 0 };
  IniFile sets(SETTINGS_FILE);
  if (!sets.open()) {
    return S_NOT_EXISTS;
  }

  if (!sets.validate(buffer, buffer_length)) {
    return S_INVALID;
  }

  if (!sets.getValue("filter", "enabled", buffer, buffer_length, settings.filter)) settings.filter = false;
  if (settings.filter) {
    if (sets.getValue("filter", "type", buffer, buffer_length)) {
      if (strcasecmp(buffer, "lp") == 0) settings.filter_type = LOWPASS;
      else if (strcasecmp(buffer, "bp") == 0) settings.filter_type = BANDPASS;
      else return S_INCORRECT_VALUE;
    } else settings.filter_type = LOWPASS;
    if (!sets.getValue("filter", "cutoff", buffer, buffer_length, settings.cutoff)) settings.cutoff = 800;
  }

  if (!sets.getValue("log", "enabled", buffer, buffer_length, settings.log)) settings.log = true;
  if (!sets.getValue("log", "write_wav", buffer, buffer_length, settings.write_wav)) settings.write_wav = true;
  if (sets.getValue("log", "file_name", buffer, buffer_length)) strcpy(settings.file_name, buffer);
  else strcpy(settings.file_name, "LOG.CSV");

  if (sets.getValue("processing", "interp_mode", buffer, buffer_length)) {
    if (strcasecmp(buffer, "none") == 0) settings.interp_mode = INTERP_NONE;
    else if (strcasecmp(buffer, "linear") == 0) settings.interp_mode = INTERP_LINEAR;
    else if (strcasecmp(buffer, "parabolic") == 0) settings.interp_mode = INTERP_PARABOLIC;
    else if (strcasecmp(buffer, "spline") == 0) settings.interp_mode = INTERP_SPLINE;
    else if (strcasecmp(buffer, "lagrange") == 0) settings.interp_mode = INTERP_LAGRANGE;
    else if (strcasecmp(buffer, "lin_par") == 0) settings.interp_mode = -1; // TODO: replace with named constant
    else if (strcasecmp(buffer, "par_lin") == 0) settings.interp_mode = -2; // TODO: replace with named constant
    else return S_INCORRECT_VALUE;
  } else {
    settings.interp_mode = INTERP_NONE;
  }

  if (sets.getValue("processing", "sampling_rate", buffer, buffer_length)) {
    if (strcasecmp(buffer, "32") == 0) settings.sampling_rate = 32000;
    else if (strcasecmp(buffer, "44.1") == 0) settings.sampling_rate = 44100;
    else if (strcasecmp(buffer, "48") == 0) settings.sampling_rate = 48000;
    else if (strcasecmp(buffer, "96") == 0) settings.sampling_rate = 96000;
    else return S_INCORRECT_VALUE;
  } else settings.sampling_rate = 44100;

  if (sets.getValue("processing", "linein_sens", buffer, buffer_length, settings.linein_sensetivity)) {
    if (settings.linein_sensetivity < 0 || settings.linein_sensetivity > 15) return S_INCORRECT_VALUE;
  } else settings.linein_sensetivity = 9;

  if (sets.getValue("processing", "level_on", buffer, buffer_length, settings.comparator_value)) {
    settings.comparator_value = std::min(0.95, (float)pow(10.0, settings.comparator_value / 20.0) * (0.775 / linein_sens[settings.linein_sensetivity]));
  } else settings.comparator_value = 0.1;

  if (!sets.getValue("processing", "enabled", buffer, buffer_length, settings.processing_enabled)) settings.processing_enabled = true;

  if (!sets.getValue("device", "base_len", buffer, buffer_length, settings.base_length)) settings.base_length = 0.34;

  return S_OK;
}

void print_settings(void) {
  Serial.println("\nCurrent settings:");
  Serial.printf("\tFilter is %s\n", current_settings.filter ? "enabled" : "disabled");
  Serial.printf("\tFilter type is %s\n", current_settings.filter_type == LOWPASS ? "lowpass" : current_settings.filter_type == BANDPASS ? "bandpass" : "<unknown>");
  Serial.printf("\tSampling rate is %2.1f kHz\n", current_settings.sampling_rate/1000.0);
  Serial.printf("\tCutoff frequency is %d Hz\n", current_settings.cutoff);
  Serial.printf("\tLineIn sensitivity is %1.2f V p-p\n", linein_sens[current_settings.linein_sensetivity]);
  Serial.printf("\tComparator value is %2.0f dB\n", 20*log10(current_settings.comparator_value));
  Serial.printf("\tLogging is %s\n", current_settings.log ? "enabled" : "disabled");
  Serial.printf("\tLog file is %s\n", current_settings.file_name);
  Serial.printf("\tRecord saving is %s\n", current_settings.write_wav ? "enabled" : "disabled");
  Serial.printf("\tInterpolation mode is %s\n", current_settings.interp_mode == INTERP_NONE ? "NONE" : current_settings.interp_mode == INTERP_LINEAR ? "LINEaR" : current_settings.interp_mode == INTERP_PARABOLIC ? "PARABOLIC" : current_settings.interp_mode == INTERP_SPLINE ? "SPLINE" : current_settings.interp_mode == INTERP_LAGRANGE ? "LAGRANGE" : current_settings.interp_mode == -1 ? "LIN_PAR" : current_settings.interp_mode == -2 ? "PAR_LIN" : "<unknown>");
  Serial.printf("\tBase length is %2.3f\n", current_settings.base_length);
}

int8_t initialize_sd_card(void) {
  SPI.setMOSI(SD_MOSI_PIN);
  SPI.setSCK(SD_SCK_PIN);

  int8_t res = 0;
  if (!card.init(SD_SHIELD_PIN)) {
    res |= SD_NOT_CONNECTED;
  }

  // int16_t type = card.type();
  // if (type != SD_CARD_TYPE_SD1 &&
  //     type != SD_CARD_TYPE_SD2 &&
  //     type != SD_CARD_TYPE_SDHC) {
  //   res |= SD_UNKNOWN_FS;
  // }

  if (!volume.init(card)) {
    res |= SD_UNABLE_ACCESS_FS;
  }

  if (!SD.begin(SD_SHIELD_PIN)) {
    res |= SD_UNABLE_ACCESS_FS;
  }

  if (res == 0) res = SD_OK;

  return res;
}

int8_t initialize_audio_subsystem(void) {
  AudioMemory(20);
  //TODO: Add FS selecting
  sgtl5000_1.enable(); //current_settings.sampling_rate);
  sgtl5000_1.inputSelect(AUDIO_INPUT_LINEIN);
  sgtl5000_1.volume(0.0);
  sgtl5000_1.muteHeadphone();

  sgtl5000_1.lineInLevel(current_settings.linein_sensetivity);

  // TODO: Add custom filters
  if (current_settings.filter) {
    switch (current_settings.cutoff) {
      case 600:
        // TODO: Add 600 Hz LP filter, BP
      break;
      case 800:
        rec.setFilter(low_pass_cut_800Hz.numerator, low_pass_cut_800Hz.order);
      break;
      case 1000:
        rec.setFilter(low_pass_cut_1000Hz.numerator, low_pass_cut_1000Hz.order);
      break;
    }
    rec.enableFilter();
  }

  rec.threshold(current_settings.comparator_value);
  rec.bypass();

  rec.begin();

  return S_OK;
}

uint32_t cnt = 1, trig_cnt = 0;
void loop() {
  int16_t st = rec.getState();

  //TODO: little refactoring are needed here
  if (cnt % 10000 == 0) {
    // Serial.printf("Current state is %d:", st);
    // switch (st) {
    //   case STATE_IDLE: Serial.println("STATE_IDLE"); break;
    //   case STATE_ENDED: Serial.println("STATE_ENDED"); break;
    //   case STATE_WAIT_SW: Serial.println("STATE_WAIT_SW"); break;
    //   case STATE_PLAY: Serial.println("STATE_PLAY"); break;
    //   case STATE_BYPASS: Serial.println("STATE_BYPASS"); break;
    //   case STATE_TRANSITIVE: Serial.println("STATE_TRANSITIVE"); break;
    // }
    if (st == STATE_TRANSITIVE) trig_cnt++;
    else trig_cnt = 0;
    cnt = 1;
  }
  else cnt++;

  if (trig_cnt > 25) {
    Serial.println("Restarting...");

    digitalWriteFast(LED, LOW);
    delay(25);
    digitalWriteFast(LED, HIGH);

    trig_cnt = 0;
    rec.clear();
    rec.begin();
    return;
  }

  if (st != STATE_ENDED) return;
  
  Serial.println("Recorded...");

  Serial.printf("Filter is %s\n", rec.filterState() ? "enabled" : "disabled");
  Serial.printf("freeMemory() = %3.3f kB\n", getFreeMemory() / 1000.0);
  Serial.printf("AudioMemoryUsage = %d, AudioMemoryUsageMax = %d\n", AudioMemoryUsage(), AudioMemoryUsageMax());

  digitalWriteFast(LED, LOW);

  long t0 = micros();
  proccessRecordedData();
  long t = micros() - t0;
  Serial.printf("%2.2f s elapsed.", t / 1000000.0);
  // proccessBearingRecord();

  // Blink twice to signalize that is all ok
  digitalWriteFast(LED, HIGH);
  delay(75);
  digitalWriteFast(LED, LOW);
  delay(75);
  digitalWriteFast(LED, HIGH);
  delay(75);
  digitalWriteFast(LED, LOW);
  delay(75);
  digitalWriteFast(LED, HIGH);

  rec.clear();
  rec.begin();

  Serial.println("\n");
  Serial.println("Waiting...");
}

float32_t Pst = 101.325; // kPa

void proccessRecordedData(void) {
  File logFile;
  if (sd_init_status == SD_OK && current_settings.log) {
    int fileNum = 0;
    if (current_settings.write_wav) {
      uint32_t t0 = micros();
      fileNum = saveWavFile();
      uint32_t dt = micros() - t0;

      Serial.printf("\tRecord saved into \"REC_%d.WAV\" file. Writing takes %5.2f ms\n", fileNum, dt / 1000.0);
    }

    if (!current_settings.processing_enabled) return;

    if (!SD.exists(current_settings.file_name)) {
      logFile = SD.open(current_settings.file_name, FILE_WRITE);
      logFile.println("No,temp,RH,c,dTOA_nr,dBW_nr,dSW_nr,alpha_nr,beta_nr,R_nr,dTOA_r,dBW_r,dSW_r,alpha_r,beta_r,R_r");
      fileNum = 1;
    } else logFile = SD.open(current_settings.file_name, FILE_WRITE);

    if (fileNum == 0) ;
    logFile.print(fileNum);
  }

  std::tuple<uint32_t, int16_t> peak_left_sw = rec.getPeak(LEFT_CHANNEL, SW);
  std::tuple<uint32_t, int16_t> peak_left_bw = rec.getPeak(LEFT_CHANNEL, BW);

  std::tuple<uint32_t, int16_t> peak_right_sw = rec.getPeak(RIGHT_CHANNEL, SW);
  std::tuple<uint32_t, int16_t> peak_right_bw = rec.getPeak(RIGHT_CHANNEL, BW);

  uint32_t dTOA_l = std::get<TIME>(peak_left_bw) - std::get<TIME>(peak_left_sw);
  uint32_t dTOA_r = std::get<TIME>(peak_right_bw) - std::get<TIME>(peak_right_sw);

  // TODO: Remove comparation, 'coz it's wrong. In this case bearing angle always will be positive!!!
  // NOTE: one must to know in which order microphones are connected to the device and how microphones are
  //       located at the acoustic base(antena).
  uint32_t dSW = std::get<TIME>(peak_left_sw) > std::get<TIME>(peak_right_sw) ? (std::get<TIME>(peak_left_sw) - std::get<TIME>(peak_right_sw)) : (std::get<TIME>(peak_right_sw) - std::get<TIME>(peak_left_sw));
  uint32_t dBW = std::get<TIME>(peak_left_bw) > std::get<TIME>(peak_right_bw) ? (std::get<TIME>(peak_left_bw) - std::get<TIME>(peak_right_bw)) : (std::get<TIME>(peak_right_bw) - std::get<TIME>(peak_left_bw));

  float32_t temp = tempSensor.readTemperature();
  float32_t hum = tempSensor.readHumidity();
  if (isnan(temp)) temp = 24.0f;
  if (isnan(hum)) hum = 70;
  Serial.printf("Temperature is %f *C.\nHumidity is %f%%\n", temp, hum);
  double c = calculateSpeedOfSound(temp, hum, Pst);
  Serial.printf("c = %3.2f m/s, d = %3.2f m\n", c, current_settings.base_length);

  if (sd_init_status == SD_OK && current_settings.log) {
    logFile.print(",");
    logFile.print(temp, 3);
    logFile.print(",");
    logFile.print(hum, 3);
    logFile.print(",");
    logFile.print(c, 3);
    logFile.print(",");
  }

  float groupDelay = rec.groupDelay();
  //NOTE: group delay compensation is under comment, 'coz it was taken into account in the update function
  float deltaTOA = (dTOA_l + dTOA_r/* - 2 * groupDelay*/) / (2.0 * AUDIO_SAMPLE_RATE_EXACT);
  float deltaBlastWave = dBW / AUDIO_SAMPLE_RATE_EXACT;
  float deltaShockWave = dSW / AUDIO_SAMPLE_RATE_EXACT;

  float alpha = asin(c * deltaBlastWave / current_settings.base_length);
  float betta = asin(c * deltaShockWave / current_settings.base_length);
  float Theta = PI/2.0 - betta;
  float R = c * deltaTOA / (1 - cos(betta - alpha));

  Serial.println("Without refining:");
  Serial.printf("\tdTOA = %f ms\n\tdBW = %f us\n\tdSW = %f us\n\talpha = %f\n\tbeta = %f\n", deltaTOA * ms, deltaBlastWave * us, deltaShockWave * us, alpha * (180.0 / PI), betta * (180.0 / PI));
  Serial.printf("\tR = %3.3f m, Theta = %3.2f\n\n", R, Theta * (180.0 / PI));

  if (sd_init_status == SD_OK && current_settings.log) {
    logFile.print(deltaTOA * ms, 4);
    logFile.print(",");
    logFile.print(deltaBlastWave * us, 4);
    logFile.print(",");
    logFile.print(deltaShockWave * us, 4);
    logFile.print(",");
    logFile.print(alpha * (180.0 / PI), 4);
    logFile.print(",");
    logFile.print(betta * (180.0 / PI), 4);
    logFile.print(",");
    logFile.print(R, 3);
    logFile.print(",");
  }

  if (current_settings.interp_mode == INTERP_NONE) {
    Serial.println("\tNo refining (interp_mode == INTERP_NONE).");

    if (sd_init_status == SD_OK && current_settings.log) {
      logFile.println("-,-,-,-,-,-");
      logFile.close();
    }

    return;
  }

  int sw_interp_mode = INTERP_LINEAR;
  int bw_interp_mode = INTERP_PARABOLIC;

  if (current_settings.interp_mode == -2) {
    sw_interp_mode = INTERP_PARABOLIC;
    bw_interp_mode = INTERP_LINEAR;
  } else if (current_settings.interp_mode == INTERP_LINEAR) {
    sw_interp_mode = INTERP_LINEAR;
    bw_interp_mode = INTERP_LINEAR;
  } else if (current_settings.interp_mode == INTERP_PARABOLIC) {
    sw_interp_mode = INTERP_PARABOLIC;
    bw_interp_mode = INTERP_PARABOLIC;
  }

  // TODO: Refactor to one call of 'refineIndexes'
  std::vector<float32_t> *refined_left_sw_3dB = rec.refineIndexes(LEFT_CHANNEL, std::get<AMPLITUDE>(peak_left_sw) * ATT_3dB, sw_interp_mode, CROSS_RISE);
  std::vector<float32_t> *refined_right_sw_3dB = rec.refineIndexes(RIGHT_CHANNEL, std::get<AMPLITUDE>(peak_right_sw) * ATT_3dB, sw_interp_mode, CROSS_RISE);

  std::vector<float32_t> *refined_left_sw_6dB = rec.refineIndexes(LEFT_CHANNEL, std::get<AMPLITUDE>(peak_left_sw) * ATT_6dB, sw_interp_mode, CROSS_FALL);
  std::vector<float32_t> *refined_right_sw_6dB = rec.refineIndexes(RIGHT_CHANNEL, std::get<AMPLITUDE>(peak_right_sw) * ATT_6dB, sw_interp_mode, CROSS_FALL);
  
  std::vector<float32_t> *refined_left_bw_3dB = rec.refineIndexes(LEFT_CHANNEL, std::get<AMPLITUDE>(peak_left_bw) * ATT_3dB, bw_interp_mode, CROSS_RISE);
  std::vector<float32_t> *refined_right_bw_3dB = rec.refineIndexes(RIGHT_CHANNEL, std::get<AMPLITUDE>(peak_right_bw) * ATT_3dB, bw_interp_mode, CROSS_RISE);

  std::vector<float32_t> *refined_left_bw_6dB = rec.refineIndexes(LEFT_CHANNEL, std::get<AMPLITUDE>(peak_left_bw) * ATT_6dB, bw_interp_mode, CROSS_FALL);
  std::vector<float32_t> *refined_right_bw_6dB = rec.refineIndexes(RIGHT_CHANNEL, std::get<AMPLITUDE>(peak_right_bw) * ATT_6dB, bw_interp_mode, CROSS_FALL);
  
  bool exists_att_3dB = refined_left_sw_3dB->size() > 0 && refined_right_sw_3dB->size() > 0 && refined_left_bw_3dB->size() > 0 && refined_right_bw_3dB->size() > 0;
  bool exists_att_6dB = refined_left_sw_6dB->size() > 0 && refined_right_sw_6dB->size() > 0 && refined_left_bw_6dB->size() > 0 && refined_right_bw_6dB->size() > 0;

  if (exists_att_3dB || exists_att_6dB)  {
    Serial.printf("freeMemory() = %3.3f kB\n", getFreeMemory() / 1000.0);
    Serial.print("Refined at -3 dB, -6 dB with ");
    switch (current_settings.interp_mode) {
      case INTERP_LINEAR: Serial.print("INTERP_LINEAR, INTERP_LINEAR"); break;
      case INTERP_PARABOLIC: Serial.print("INTERP_PARABOLIC, INTERP_PARABOLIC"); break;
      case INTERP_SPLINE: Serial.print("INTERP_SPLINE, INTERP_SPLINE"); break;
      case INTERP_LAGRANGE: Serial.print("INTERP_LAGRANGE, INTERP_LAGRANGE"); break;
      case -1: Serial.print("INTERP_LINEAR, INTERP_PARABOLIC"); break;
      case -2: Serial.print("INTERP_PARABOLIC, INTERP_LINEAR"); break;
    }
    Serial.println(":");

    float32_t rdTOA_l_3, rdTOA_r_3, rdBW_3, rdSW_3, rdTOA_l_6, rdTOA_r_6, rdBW_6, rdSW_6;

    // if exists refined indexes at -3dB or(and) -6dB, than calculates it's mean value
    if (exists_att_3dB && !exists_att_6dB) {
      rdTOA_l_6 = rdTOA_l_3 = refined_left_bw_3dB->back() - refined_left_sw_3dB->front();
      rdTOA_r_6 = rdTOA_r_3 = refined_right_bw_3dB->back() - refined_right_sw_3dB->front();
      rdBW_6 = rdBW_3 = refined_left_bw_3dB->back() - refined_right_bw_3dB->back();
      rdSW_6 = rdSW_3 = refined_left_sw_3dB->front() - refined_right_sw_3dB->front();
    } 
    else if (!exists_att_3dB && exists_att_6dB) {
      rdTOA_l_6 = rdTOA_l_3 = refined_left_bw_6dB->back() - refined_left_sw_6dB->front();
      rdTOA_r_6 = rdTOA_r_3 = refined_right_bw_6dB->back() - refined_right_sw_6dB->front();
      rdBW_6 = rdBW_3 = refined_left_bw_6dB->back() - refined_right_bw_6dB->back();
      rdSW_6 = rdSW_3 = refined_left_sw_6dB->front() - refined_right_sw_6dB->front();
    }
    else {
      rdTOA_l_3 = refined_left_bw_3dB->back() - refined_left_sw_3dB->front();
      rdTOA_r_3 = refined_right_bw_3dB->back() - refined_right_sw_3dB->front();
      rdBW_3 = refined_left_bw_3dB->back() - refined_right_bw_3dB->back();
      rdSW_3 = refined_left_sw_3dB->front() - refined_right_sw_3dB->front();

      rdTOA_l_6 = refined_left_bw_6dB->back() - refined_left_sw_6dB->front();
      rdTOA_r_6 = refined_right_bw_6dB->back() - refined_right_sw_6dB->front();
      rdBW_6 = refined_left_bw_6dB->back() - refined_right_bw_6dB->back();
      rdSW_6 = refined_left_sw_6dB->front() - refined_right_sw_6dB->front();
    }

    float32_t rdTOA_l = (rdTOA_l_3 + rdTOA_l_6) / 2.0;
    float32_t rdTOA_r =(rdTOA_r_3 + rdTOA_r_6) / 2.0;
    float32_t rdBW = (rdBW_3 + rdBW_6) / 2.0;
    float32_t rdSW = (rdSW_3 + rdSW_6) / 2.0;

    //TODO: Watch for group delay compensation!
    //NOTE: Here are needed group delay compensation, 'coz refineIndexes calucates one without group delay compensation 
    float32_t rdeltaTOA = (rdTOA_l + rdTOA_r - 2 * groupDelay) / (2.0 * AUDIO_SAMPLE_RATE_EXACT);
    float32_t rdeltaBlastWave = rdBW / AUDIO_SAMPLE_RATE_EXACT;
    float32_t rdeltaShockWave = rdSW / AUDIO_SAMPLE_RATE_EXACT;

    float32_t ralpha = asin(c * rdeltaBlastWave / current_settings.base_length);
    float32_t rbetta = asin(c * rdeltaShockWave / current_settings.base_length);
    float32_t rTheta = PI/2.0 - rbetta;
    float32_t rR = c * deltaTOA / (1 - cos(rbetta - ralpha));

    Serial.printf("\tdTOA = %f ms\n\tdBW = %f us\n\tdSW = %f us\n\talpha = %f\n\tbeta = %f\n", rdeltaTOA*ms, rdeltaBlastWave*us, rdeltaShockWave*us, ralpha * (180.0 / PI), rbetta * (180.0 / PI));
    Serial.printf("\tR = %3.3f m, Theta = %3.2f\n\n", rR, rTheta * (180.0 / PI));

    if (sd_init_status == SD_OK && current_settings.log) {
      logFile.print(rdeltaTOA * ms, 4);
      logFile.print(",");
      logFile.print(rdeltaBlastWave * us, 4);
      logFile.print(",");
      logFile.print(rdeltaShockWave * us, 4);
      logFile.print(",");
      logFile.print(ralpha * (180.0 / PI), 4);
      logFile.print(",");
      logFile.print(rbetta * (180.0 / PI), 4);
      logFile.print(",");
      logFile.print(rR, 3);
      logFile.println();
    }
  } else {
    Serial.println("\tCan't refine.");

    if (sd_init_status == SD_OK && current_settings.log) {
      logFile.println("-,-,-,-,-,-");
    }
  }

  if (sd_init_status == SD_OK && current_settings.log) {
    logFile.println();
    logFile.close();
  }

  refined_left_sw_3dB->clear();
  refined_right_sw_3dB->clear();
  refined_left_bw_3dB->clear();
  refined_right_bw_3dB->clear();
  
  refined_left_sw_6dB->clear();
  refined_right_sw_6dB->clear();
  refined_left_bw_6dB->clear();
  refined_right_bw_6dB->clear();

  delete refined_left_sw_3dB;
  delete refined_right_sw_3dB;
  delete refined_left_bw_3dB;
  delete refined_right_bw_3dB;

  delete refined_left_sw_6dB;
  delete refined_right_sw_6dB;
  delete refined_left_bw_6dB;
  delete refined_right_bw_6dB;
}

// [Obsolete]
void proccessBearingRecord(void) {
  std::tuple<uint32_t, int16_t> peak_left_sw = rec.getPeak(LEFT_CHANNEL, 1);
  std::tuple<uint32_t, int16_t> peak_right_sw = rec.getPeak(RIGHT_CHANNEL, 1);

  Serial.printf("p1[LEFT] = %lu, p2[LEFT] = %lu\n", std::get<0>(peak_left_sw), std::get<0>(peak_right_sw));
  uint32_t dTOA = std::get<0>(peak_left_sw) > std::get<0>(peak_right_sw) ? (std::get<0>(peak_left_sw) - std::get<0>(peak_right_sw)) : (std::get<0>(peak_right_sw) - std::get<0>(peak_left_sw));
  
  File logFile;
  if (sd_init_status == SD_OK) {
    int fileNum = saveWavFile();
    logFile = SD.open("LOG.TXT", FILE_WRITE);
    logFile.print(fileNum);
  }

  float32_t temp = tempSensor.readTemperature();
  float32_t hum = tempSensor.readHumidity();
  if (isnan(temp)) temp = 24.0f;
  if (isnan(hum)) hum = 70;
  Serial.printf("Temperature is %f *C.\nHumidity is %f%%\n", temp, hum);
  double c = calculateSpeedOfSound(temp, hum, Pst);
  Serial.printf("c = %3.2f m/s, d = %3.2f m\n", c, current_settings.base_length);
  //Serial.printf("but c = %3.2f\n", c = 340);

  if (sd_init_status == SD_OK) {
    logFile.print("\tt* = ");
    logFile.print(temp, 3);
    logFile.print(" *C\th = ");
    logFile.print(hum, 3);
    logFile.print("%%\tc = ");
    logFile.print(c, 3);
    logFile.print(" m/s\td = ");
    logFile.print(current_settings.base_length, 3);
  }

  Serial.printf("c = %3.2f m/s, d = %3.2f m\n", c, current_settings.base_length);

  float deltaTimeOfArrival = dTOA / AUDIO_SAMPLE_RATE_EXACT;

  float alpha = asin(c * deltaTimeOfArrival / current_settings.base_length);

  Serial.printf("Bearing is %3.2f deg\n", alpha * (180.0 / PI));

  std::vector<float32_t> *refined_left_1 = rec.refineIndexes(LEFT_CHANNEL, std::get<1>(peak_left_sw) * ATT_3dB, INTERP_LINEAR, CROSS_RISE);
  std::vector<float32_t> *refined_right_1 = rec.refineIndexes(RIGHT_CHANNEL, std::get<1>(peak_right_sw) * ATT_3dB, INTERP_LINEAR, CROSS_RISE);
  
  if (refined_left_1->size() > 0 && refined_right_1->size() > 0) {
    Serial.printf("Refined with INTERP_LINEAR p2[RIGHT] = %f\n", refined_right_1->back());
    for (std::vector<float32_t>::iterator it = refined_right_1->begin(); it != refined_right_1->end(); ++it) {
      Serial.printf("%5.3f, ", *it);
    }
    Serial.println();

    float32_t rdSW = refined_left_1->front() - refined_right_1->front();
    float32_t rdeltaShockWave = rdSW / AUDIO_SAMPLE_RATE_EXACT;

    float32_t ralpha = asin(c * rdeltaShockWave / current_settings.base_length);

    Serial.println("Refined linear:");
    Serial.printf("Bearing is %3.2f deg\n", ralpha * (180.0 / PI));


    if (sd_init_status == SD_OK) {
      logFile.print(" us\tdT = ");
      logFile.print(rdeltaShockWave * us, 4);
      logFile.print(" us\talpha = ");
      logFile.print(ralpha * (180.0 / PI), 4);
      logFile.println();
    }
  } else {
    Serial.println("\tCan't refine.");

    if (sd_init_status == SD_OK) {
      logFile.print("\tdT = ");
      logFile.print(deltaTimeOfArrival * us, 4);
      logFile.print(" us\talpha = ");
      logFile.print(alpha * (180.0 / PI), 4);
      logFile.println();
    }
  }

  refined_left_1->clear();
  refined_right_1->clear();

  delete refined_left_1;
  delete refined_right_1;
}

template <typename Word> void write_word(File f, Word value, unsigned size = sizeof(Word))
{
  for (; size; --size, value >>= 8)
    f.write(static_cast<char>(value & 0xFF));
}

int saveWavFile(void) {
  //Find last file
  int i = 1;
  char fname[11] = "";
  do {
    sprintf(fname, "REC_%04d.WAV", i);
    // String num = String(i);
    // fname = rec_str + num + ".WAV";
    i++;
  } while (SD.exists(fname));
  File w = SD.open(fname, FILE_WRITE);

  uint32_t length = rec.bufferLength();
  uint32_t file_length = 36 + length * 2 * 2;

  // Write the file headers
  w.write("RIFF");
  write_word(w, file_length - 8, 4); 
  w.write("WAVEfmt ");
  write_word(w,     16, 4);  // no extension data
  write_word(w,      1, 2);  // PCM - integer samples
  write_word(w,      2, 2);  // two channels (stereo file)
  write_word(w,  44100, 4);  // samples per second (Hz)
  write_word(w, 176400, 4);  // (Sample Rate * BitsPerSample * Channels) / 8
  write_word(w,      4, 2);  // data block size (size of two integer samples, one for each channel, in bytes)
  write_word(w,     16, 2);  // number of bits per sample (use a multiple of 8)

  // Write the data chunk header
  size_t data_chunk_pos = 36;
  w.write("data");
  write_word(w, file_length - data_chunk_pos + 8, 4);

  //NOTE: Next lines of code are commented because it works quite not correct.
  //      In the damped .wav files on the SDHC card one got rubish instead of
  //      normal shot wave and blast wave.

  // uint16_t head = rec.getQueueHeadIndex();
  // uint32_t memory_begin = 0; //memory.memoryBegin();
  // SPI.beginTransaction(SPISETTING);
  // digitalWriteFast(SPIRAM_CS_PIN, LOW);       // begin receiving data

  // for (uint32_t j = 0; j < length; ++j) {

  //   uint32_t mem_offset = ((j + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * LEFT_CHANNEL;
  //   uint32_t addr = 2 * (memory_begin + mem_offset);
  //   SPI.transfer16((0x03 << 8) | (addr >> 16)); // Set higt(addr)
  //   SPI.transfer16(addr & 0xFFFF);              // Set low(addr)

  //   int16_t lv = (int16_t)(SPI.transfer16(0)); //rec.read(LEFT_CHANNEL, j);

  //   mem_offset = ((j + head * AUDIO_BLOCK_SAMPLES) % (QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES)) + QUEUE_LENGTH * AUDIO_BLOCK_SAMPLES * RIGHT_CHANNEL;
  //   addr = 2 * (memory_begin + mem_offset);
  //   SPI.transfer16((0x03 << 8) | (addr >> 16)); // Set higt(addr)
  //   SPI.transfer16(addr & 0xFFFF);              // Set low(addr)

  //   int16_t rv = (int16_t)(SPI.transfer16(0)); //rec.read(RIGHT_CHANNEL, j);

  //   write_word(w, lv, 2);
  //   write_word(w, rv, 2);
  // }

  // digitalWriteFast(SPIRAM_CS_PIN, HIGH);    // end receiving data
  // SPI.endTransaction();



  //TODO: Here is needed some optimization, because SDHC card of class 10 has ability to write data at high speed, right up to 10 MB/s.
  //      So, 870 ms buffer may be written in a 14.63 ms instead of ~1.9 s that one got now.
  //      Huge overhead by time we've got because of calling GateRec.read(...) and write_word(...).
  //      It is possible to inline those functions into saveWavFile(...) and use block-by-block 
  //      writing to the SDHC card instead of element-by-element.
  for (uint32_t j = 0; j < length; ++j) {
    int16_t lv = rec.read(LEFT_CHANNEL, j);
    int16_t rv = rec.read(RIGHT_CHANNEL, j);

    write_word(w, lv, 2);
    write_word(w, rv, 2);
  }

  w.close();
  return i - 1;
}
