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

#ifndef _LORAWAN_H
#define _LORAWAN_H

#include <Arduino.h>
#include <avr/dtostrf.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

// enable ADR (adaptive rate) only makes
// sense for nodes which are not moving
#define LORAWAN_ADR

// enable or disable LMIC LinkCheckMode (enable by default in LMIC)
// this can eventually lead to EV_LINK_DEAD if sensor is unable to 
// receive any downlink messages; however it can also trigger a rejoin
// https://forum.mcci.io/t/lmic-setlinkcheckmode-questions/96/2
#define LORAWAN_LINKCHECK

// sync RTC with LoRaWAN network time
// tested with TTN Stack v3 and ChirpStack
#define LORAWAN_NETWORKTIME

// send battery level if requested by LNS
#define LORAWAN_MAC_BATLEVEL

// to allow changes or different payloads send a version number
#define LORAWAN_PAYLOAD_VERSION 2

enum lmic_states {
    NONE,
    IDLE,
    JOINED,
    TXPENDING,
    TXDONE,
    NOTJOINED,
    ERROR
};

extern lmic_states lmic_status;

void lmic_init();
void lmic_send();
bool lmic_join(uint8_t repeat);
void lmic_clear();
uint8_t os_getBattLevel(void);

#endif