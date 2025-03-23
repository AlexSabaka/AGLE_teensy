# Gunshot Locator Device

## Overview

This repository contains the source code for a portable acoustic gunshot locator device. The system automatically detects gunshots, analyzes the acoustic signature, and calculates the direction and distance to the source of the gunfire. [Article Source](./ELCONF-2016_sbornik_64-68.pdf)

## How It Works

The device operates by capturing and analyzing two distinct acoustic components of a gunshot:

- **Shock wave** - The N-shaped wave produced by supersonic bullets
- **Muzzle blast** - The explosive acoustic wave from the weapon discharge

Using a dual-microphone setup with a 34 cm acoustic base, the system calculates:

- Bearing angle to the shooter
- Distance to the shooter
- (Optional) Classification of weapon type

## Hardware Components

- **Microcontroller**: ARM Cortex-M4 (MK20DX256) @ 96 MHz
- **Audio Module**: SGTL5000 DAC (16-bit, 44.1 kHz sampling)
- **Microphones**: MKE-3 electret or compatible dynamic microphones
- **Environmental Sensor**: DHT22 (temperature/humidity)
- **Memory**:
  - 23LC1024 external RAM
  - MicroSD card for data storage
- **Power**: 1200 mAh Li-ion battery with TP4056 charge controller

## Features

- Compact design with simple user interface
- Environmental compensation for sound speed variations
- Real-time signal processing and location calculation
- Data logging to microSD card
- USB connectivity for data retrieval and device programming
- Battery-powered operation with LED status indicators

## Algorithm

The system implements:

1. Circular buffer recording (250ms @ 44.1kHz)
2. Automatic threshold detection based on ambient noise level
3. Shock wave and muzzle blast identification
4. Time-of-arrival difference calculation between microphones
5. Linear interpolation for high-precision timing
6. Speed of sound calculation based on temperature and humidity
7. Trigonometric calculations for position estimation

## Performance

Laboratory tests confirmed:

- Bearing angle accuracy within ±2°
- Distance accuracy within 5 meters

## Installation & Usage

1. Clone this repository
2. Install required libraries (STL, Arduino, AudioLibrary)
3. Connect hardware components according to the schematic
4. Compile and upload code to the microcontroller

## License

GPL-3.0 license

## References

1. Peter Volgyesi, Gyorgy Balogh, Andras Nadas, Christopher B. Nash, Akos Ledeczi, "Shooter Localization and Weapon Classification with Soldier-Wearable Networked Sensors"
2. T. Damarla, "Battlefield Acoustics"
3. W. Choi, "Selective Background Adaptation Based Abnormal Acoustic Event Recognition for Audio Surveillance"
4. L. Gerosa, "Scream and Gunshot Detection in Noisy Environments"
5. Robert C. Maher, "Modeling and Signal Processing of Acoustic Gunshot Recordings"
6. Owen Cramer, "The variation of the specific heat ratio and the speed in air with temperature, pressure, humidity, and CO₂ concentration"
