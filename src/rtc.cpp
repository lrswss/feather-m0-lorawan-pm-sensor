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

#include "rtc.h"
#include "utils.h"
#include "lorawan.h"

RTCZero rtc;


// put MCU to sleep for given number of seconds
// by setting an alarm using its RTC
void sleep(uint16_t secs) {
    rtc.setAlarmEpoch(rtc.getEpoch() + secs);
    log_msg("Sleeping for %d seconds, wake up at %02d:%02d:%02d (UTC)...", 
        secs, rtc.getAlarmHours(), rtc.getAlarmMinutes(), rtc.getAlarmSeconds());
    rtc.enableAlarm(rtc.MATCH_HHMMSS);
    Serial1.flush();
    rtc.standbyMode();

    blink_led(250, 2);
    log_msg("Waking up...");
}