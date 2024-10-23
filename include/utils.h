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

#ifndef _UTILS_H
#define _UTILS_H

#include <Arduino.h>

#define serial Serial1
#define MAX_MSG 128

void blink_led(uint16_t pause, uint8_t blinks);
void log_msg(const char *fmt, ...);
void print_hex(uint8_t *arr, uint8_t len, bool ln, bool reverse);
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max);
#endif