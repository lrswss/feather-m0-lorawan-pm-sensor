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

#ifndef _CONFIG_H
#define _CONFIG_H

#include "pins.h"

// schedule observation and LoRaAWAN TX every given number of seconds
// Feather M0 will sleep inbetween transmissions to save battery
#define OBSERVATION_INTERVAL_SECS 600

// OTAA/ABP: byte array(8), little endian format (LSB)
#define LORAWAN_DEV_EUI { 0x11, 0x22, 0x33, 0x44, 0x08, 0x79, 0x30, 0x70 } 

// OTAA: byte array(8), little endian format (LSB)
#define LORAWAN_APP_EUI { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }

// OTAA: byte array(16), big endian format (MSB)
#define LORAWAN_APP_KEY { 0x22, 0xE2, 0x19, 0x88, 0x13, 0x57, 0xCC, 0x77, 0x2D, 0x18, 0x81, 0x3B, 0x8D, 0xC3, 0x45, 0x11 } 

// speed for serial monitor on RX0/TX1 (requires UART to USB adapter)
// undefine to disable serial output
#define SERIAL_BAUD 9600

// for testing without sensors
//#define NOSENSORS

#endif
