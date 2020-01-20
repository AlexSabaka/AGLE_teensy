#include <Audio.h>

#include "GateRec.h"
#include "Filter.h"


#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#include <MemoryFree.h>

#include <IniFile.h>

#include "DHT.h"


#define MODE_MINUS_3DB 1
#define MODE_N_PLUS_6DB 2

#define SQRT_2 1.4142135623730950488016887242097f
#define ATT_3dB 0.70710678118654752440084436210485f
#define ATT_6dB 0.5f
#define us 1000000.0f
#define ms 1000.0f

#define SD_MOSI_PIN 7
#define SD_SCK_PIN 14
#define SD_SHIELD_PIN 10

#define S_OK 1
#define S_NOT_EXISTS 2
#define S_INVALID 3
#define S_INCORRECT_VALUE 4

#define SD_OK 1
#define SD_NOT_CONNECTED (1 << 1)
#define SD_UNABLE_ACCESS_FS (1 << 2)
#define SD_UNKNOWN_FS (1 << 3)

#define LOWPASS 1
#define BANDPASS 2

#define SW 0
#define BW 1

#define TIME 0
#define AMPLITUDE 1

#define LED A6

#include "device_settings.h"


#include "Sound.h"
