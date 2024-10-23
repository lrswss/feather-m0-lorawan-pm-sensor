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

#ifndef _SDS011_H
#define _SDS011_H

#include "Arduino.h"
#include "wiring_private.h"

#define WARMUP_SECS 20
#define READ_TIMEOUT_MS 1000
#define CMD_RETRY 3
#define CMD_RETRY_MS 500
#define AVG_READINGS 3
#define AVG_READINGS_MS 1500
//#define SDS_DEBUG

class SDS011 {
	public:
		SDS011(uint8_t secs);
		void begin();
        bool ready();
		bool poll(float *pm25, float *pm10, uint8_t repeat = 0);
        bool info(char *version, uint16_t& id);
		bool wakeup();
        bool sleep();
	private:
        uint8_t rxbuf[10]; // SDS011 reponse has 10 byte
        uint32_t startTime;
        uint8_t warmupSecs;
        bool read(uint8_t cmd, uint8_t data1 = 0);
        bool passiveMode();
        uint8_t calcCRC(uint8_t *buf);
        bool checkCRC(uint8_t *buf, uint8_t crc);
        bool cmd(const uint8_t *cmd, const char *name);
};

#endif