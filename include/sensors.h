/***************************************************************************
  Copyright (c) 2024 Lars Wessels

  This file is part of the "Feather-M0-LoRaWAN-PM-Sensor" source code.
  https://github.com/lrswss/feather-m0-lorawan-pm-sensor

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
   
  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

***************************************************************************/

#ifndef _SENSORS_H
#define _SENSORS_H

#include <Arduino.h>
#include "config.h"
#include <avr/dtostrf.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_SHT31.h>
#include <Adafruit_Si7021.h>
#include "sds011.h"


// sensors will be tried in the following order: BME280, SHT31, SI7021

// BMP/BME280 I2C address
#define BMP_BME280_ADDRESS 0x76

// I2C address for SHT31 (temperature/humidity sensor)
#define SHT31_ADDRESS 0x44

// I2C address for SI7021 / SHT21 (temperature/humidity sensor)
#define SI7021_ADDRESS 0x40

// set according to values of voltage divider on VBAT_PIN
#define VBAT_MULTIPLIER 2.0
#define VBAT_MIN_LEVEL 3.5
#define VBAT_MAX_LEVEL 4.21

typedef struct  {
    float temperature;
    float pressure;
    int8_t humidity;
    float pm10;
    float pm25;
    double vbat;
    byte status;
} sensorReadings_t;

enum sensorStatus {
    SENSORS_OFFLINE = 0x00,
    SENSORS_INITED = 0x01,
    SENSORS_WARMUP = 0x02,
    SENSORS_I2C_FAILED = 0x04,
    SENSORS_I2C_ERROR = 0x08,
    SENSORS_SDS011_ERROR = 0x10,
    SENSORS_HAS_BME280 = 0x20,
    SENSORS_HAS_SHT31 = 0x40,
    SENSORS_HAS_SI7021 = 0x80
};

extern sensorReadings_t sensorReadings;

void sensors_init();
void sensors_read(bool verbose);
void sensors_off();
void sensors_warmup();
bool sensors_ready();
bool sensors_error();
bool vbat_read(bool verbose);

#endif