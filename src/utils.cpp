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

#include "config.h"
#include "utils.h"
#include "lorawan.h"
#include "sensors.h"
#include "rtc.h"

// for log messages
static char msg[MAX_MSG];


// blink system LED
void blink_led(uint16_t pause, uint8_t blinks) {
#ifdef LED_PIN
    for (uint8_t i = 0; i < blinks; i++) {
        digitalWrite(LED_PIN, HIGH);
        delay(pause);
        digitalWrite(LED_PIN, LOW);
        delay(pause);
    }
#endif
}


// print log messages with current LMIC ticks and RTC timestamp (UTC) 
// after LoRaWAN DeviceTimeReq was answered. Requires inited LMIC stack.
void log_msg(const char *fmt, ...) {
#ifdef SERIAL_BAUD
    snprintf(msg, MAX_MSG, "[%02d:%02d:%02d|", rtc.getHours(), rtc.getMinutes(), rtc.getSeconds());
    serial.write(msg, strlen(msg));
    if (lmic_status == NONE)
        snprintf(msg, MAX_MSG, "%.8ld] ", millis());
    else
        snprintf(msg, MAX_MSG, "%.8ld] ", os_getTime()/100);  // LMIC ticks to ms
    serial.write(msg, strlen(msg));
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, MAX_MSG, fmt, args);
    va_end(args);
    serial.write(msg, strlen(msg));
    serial.println();
#endif
}


// turn hex byte array of given length into a null-terminated hex string
static void array2string(const byte *arr, int len, char *buf, bool reverse) {
    for (int i = 0; i < len; i++)
        sprintf(buf + i * 2, "%02X", reverse ? arr[len-1-i]: arr[i]);
}


// print byte array as hex string, optionally masking last 4 bytes
void print_hex(uint8_t *arr, uint8_t len, bool ln, bool reverse) {
    char hex[len * 2 + 1];
    array2string(arr, len, hex, reverse);
    serial.print(hex);
    if (ln)
        serial.println();
}

// https://forum.arduino.cc/t/map-to-floating-point/3976/3
float mapfloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}