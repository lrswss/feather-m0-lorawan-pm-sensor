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

#include "sds011.h"
#include "utils.h"
#include "pins.h"

// stubs for relevant SDS011 serial commands
static const uint8_t CMD_SLEEP[5] = { 0xAA, 0xB4, 0x06, 0x01, 0x00 };
static const uint8_t CMD_WAKEUP[5] = { 0xAA, 0xB4, 0x06, 0x01, 0x01 };
static const uint8_t CMD_PASSIVE[5] = { 0xAA, 0xB4, 0x02, 0x01, 0x01 };
static const uint8_t CMD_QUERY[5] = { 0xAA, 0xB4, 0x04, 0x00, 0x00 };
static const uint8_t CMD_VERSION[5] = { 0xAA, 0xB4, 0x07, 0x00, 0x00 };


// SERCOM muxing on M0 for additional UART, SPI, I2C ports
// SERCOM1 can be assigned on Feather M0 LoRa (D10-D13) not used
//
// Pin       SERCOM
// -------------------
// PA18D10   SERCOM1.2
// PA16D11   SERCOM1.0
// PA19D12   SERCOM1.3
// PA17D13   SERCOM1.1
//
// RX options: SERCOM_RX_PAD_0, SERCOM_RX_PAD_1, SERCOM_RX_PAD_2, SERCOM_RX_PAD_3
// TX options: UART_TX_PAD_0, UART_TX_PAD_2

// setup additioanl serial port on Pin 11 (Rx) and Pin 10 (Tx)
Uart Serial2 (&sercom1, SDS011_TX_PIN, SDS011_RX_PIN, SERCOM_RX_PAD_0, UART_TX_PAD_2);


void SERCOM1_Handler() {
    Serial2.IrqHandler();
}


SDS011::SDS011(uint8_t secs) {
    warmupSecs = secs;
}


// SDS011 is activate (fan spin up, laser diode on) an setup for 'report query mode'
// It reports PM reading on explicit 'query data command' (see command stubs above)
void SDS011::begin() {
    Serial2.begin(9600, SERIAL_8N1);
    pinPeripheral(10, PIO_SERCOM);
    pinPeripheral(11, PIO_SERCOM);
    this->wakeup();
    this->passiveMode();
    startTime = millis();
}


// returns PM readings; sends query command to SDS011 and reads response
// return false if warmup time (fan running) has not been reached
// if parameter 'repeat' (max. 5) is set, average results after given number
// of consecutive readings; prolongs poll time (DELAY_AVG_READINGS_MS * repeat)
bool SDS011::poll(float *pm25, float *pm10, uint8_t repeat) {
    if (!this->ready())
        return false;

    *pm25 = 0; *pm10 = 0; repeat++;
    for (uint8_t i = 1; i < repeat; i++) {
        this->cmd(CMD_QUERY, "poll");
        if (this->read(0xC0)) {
            *pm25 += (rxbuf[3] << 8 | rxbuf[2]) / 10.0;
            *pm10 += (rxbuf[5] << 8 | rxbuf[4]) / 10.0;
            if (i == repeat-1) {
                *pm25 /= i;
                *pm10 /= i;
                return true;
            }
        } else {
            return false;
        }
        delay(AVG_READINGS_MS);
    }
    return false;
}


// return firmware version and device id
bool SDS011::info(char *version, uint16_t& id) {
    for (uint8_t i = 0; i < CMD_RETRY; i++) {
        this->cmd(CMD_VERSION, "info");
        if (this->read(0xC5, 0x07)) {
            sprintf(version, "%02u%02u%02u", rxbuf[3] % 100, rxbuf[4] % 100, rxbuf[5] % 100);
            id = (static_cast<uint16_t>(rxbuf[6]) << 8) + rxbuf[7];
            return true;
        }
        delay(CMD_RETRY_MS);
    }
    return false;
}


// send SDS011 to sleep (turns of fan and laser diode)
// power consumption < 4mA
bool SDS011::sleep() {
    startTime = 0;
    for (uint8_t i = 0; i < CMD_RETRY; i++) {
        this->cmd(CMD_SLEEP, "sleep");
        if (this->read(0xC5, 0x06))
            return true;
        delay(CMD_RETRY_MS);
    }
    return false;
}


// wake up SDS011 (fan spins up, laser diode on)
bool SDS011::wakeup() {
    startTime = millis();
    for (uint8_t i = 0; i < CMD_RETRY; i++) {
        this->cmd(CMD_WAKEUP, "wakeup");
        if (this->read(0xC5, 0x06))
            return true;
        delay(CMD_RETRY_MS);
    }
    return false;
}


// switch passive reporting mode
// SDS011 will only return reading after explicit query
bool SDS011::passiveMode() {
    for (uint8_t i = 0; i < CMD_RETRY; i++) {
        this->cmd(CMD_PASSIVE, "passiveMode");
        if (this->read(0xC5, 0x02))
            return true;
        delay(CMD_RETRY_MS);
    }
    return false;
}


// read response on serial port after sending SDS011:cmd()
bool SDS011::read(uint8_t cmd, uint8_t data1) {
    uint8_t i = 0, timeout = 0;
    uint32_t startRead = millis();

#ifdef SDS_DEBUG
    Serial1.printf("SDS011::read(%.2X): ", cmd);
#endif
    memset(rxbuf, 0, sizeof(rxbuf));
	while (++timeout < READ_TIMEOUT_MS && i < 10) {
        if (Serial2.available() > 0) {
		    rxbuf[i++] = Serial2.read();
#ifdef SDS_DEBUG
            Serial1.printf("%.2X ", rxbuf[i-1]);
#endif
            switch (i-1) {
                case 0: if (rxbuf[0] != 0xAA) { i = 0; } break;
                case 1: if (rxbuf[1] != cmd) { i = 0; } break;
                case 2: if (cmd != 0xC0 && rxbuf[2] != data1) { i = 0; } break;
                case 8: if (!this->checkCRC(rxbuf, rxbuf[8])) { i = 0; } break;
                case 9: if (rxbuf[9] != 0xAB) { i = 0; } break;
            }
        }
        delay(1);
    }

    if (i != 10) {
#ifdef SDS_DEBUG
        Serial1.println();
#endif
        log_msg("[WARNING] SDS011 read timeout!");
        return false;
    } 
#ifdef SDS_DEBUG
    else {
        Serial1.printf("(%d ms)\n", millis() - startRead);
    }
#endif
    return true;
}


// ensure warmup time (fan running) before reading PM values
bool SDS011::ready() {
    uint32_t runSecs = (millis() - startTime) / 1000;
    if ((millis() % 1000) == 0) {
        log_msg("SDS011 warming up (%d secs)...", (warmupSecs - runSecs));
        delay(1);
    }
    return startTime == 0 ? false : runSecs >= warmupSecs;
}


uint8_t SDS011::calcCRC(uint8_t *buf) {
    uint8_t crc = 0;

    for (uint8_t i = 2; i < 17; ++i) {
        crc += buf[i];
    }
    return crc;
}


bool SDS011::checkCRC(uint8_t *buf, uint8_t crc) {
    uint8_t crc_calc = 0;

    for (uint8_t i = 2; i < 8; ++i) {
        crc_calc += buf[i];
    }
    return crc == crc_calc;
}


// send predefined command sequence with checksum to SDS011 (see cmd stubs above)
bool SDS011::cmd(const uint8_t *cmd, const char *name) {
    static uint8_t buf[19];

    memset(buf, 0, sizeof(buf));
    for (uint8_t i = 0; i < 5; i++) {
        buf[i] = cmd[i];
    }
    buf[15] = 0xFF;
    buf[16] = 0xFF;
    buf[17] = this->calcCRC(buf);
    buf[18] = 0xAB;

#ifdef SDS_DEBUG
    Serial1.printf("SDS011::cmd(%s) ", name);
    for (uint8_t i = 0; i < 19; i++) {
        Serial1.printf("%.2X ", buf[i]);
    }
    Serial1.println();
#endif
    return Serial2.write(buf, sizeof(buf)) == sizeof(buf);
}