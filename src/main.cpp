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

#include "lorawan.h"
#include "utils.h"
#include "sensors.h"
#include "config.h"
#include "rtc.h"


void setup() {
    rtc.begin();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);
    blink_led(250, 2);

    // RX0/TX1 is used as serial monitor (requires UART to USB adapter)
    // not using USB port since it doesn't survive deep sleep mode
#ifdef SERIAL_BAUD
    serial.begin(SERIAL_BAUD);
    while (!serial);
    serial.println();
    log_msg("Feather M0 LoRaWAN Dust Sensor v%d starting...", FIRMWARE_VERSION);
#endif
    vbat_read(true);
    sensors_init();
    sensors_off(); // spin down SDS011 to save power (~110mA)
    lmic_init();
}


void loop() {
    // after 3 failed join request goto sleep
    if (!lmic_join(3)) {
        sensors_off();
        sleep(OBSERVATION_INTERVAL_SECS);

    // warmup sensors (turn on SDS011 fan and laser diode) after join
    } else if (lmic_status == JOINED && !(sensorReadings.status & SENSORS_WARMUP)) {
        sensors_warmup();

    // after transmitting sensor readings goto sleep
    } else if (lmic_status >= TXDONE) {
        lmic_clear();
        sleep(OBSERVATION_INTERVAL_SECS);
        sensors_warmup(); // warmup sensor afer wakeup

    // report sensor error status
    } else if (lmic_status < TXPENDING && sensors_error()) {
        vbat_read(true);
        lmic_send();

    // if no tranmission is pending and sensors are ready,
    // read sensor values and trigger transmission
    } else if (lmic_status < TXPENDING && sensors_ready()) {
        sensors_read(true);
        vbat_read(true);
        sensors_off(); // spin down SDS011 to save power
        lmic_send();
    }

    os_runloop_once();
}